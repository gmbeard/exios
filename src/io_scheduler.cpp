#include "exios/io_scheduler.hpp"
#include "exios/async_io_operation.hpp"
#include "exios/contracts.hpp"
#include <algorithm>
#include <cerrno>
#include <errno.h>
#include <span>
#include <sys/epoll.h>
#include <unistd.h>

namespace
{
constexpr std::size_t kMaxEventsPerPoll = 1024;
}

namespace exios
{
IoScheduler::IoScheduler()
    : epoll_fd_ { epoll_create1(EPOLL_CLOEXEC) }
{
    if (epoll_fd_ < 0)
        throw std::system_error { errno, std::system_category() };

    event_buffer_.resize(kMaxEventsPerPoll);
}

IoScheduler::~IoScheduler()
{
    ::close(epoll_fd_);
    drain_list(operations_, [](auto& item) { item.discard(); });
}

auto IoScheduler::schedule(AsyncIoOperation* op) noexcept -> void
{
    /* Operations are added in order of their `fd`...
     */

    /* TODO: Thread safety...
     */
    static_cast<void>(
        operations_.insert(op,
                           std::lower_bound(operations_.begin(),
                                            operations_.end(),
                                            op,
                                            [](auto const& a, auto const* b) {
                                                return a.get_fd() < b->get_fd();
                                            })));
}

auto IoScheduler::poll_once() -> void
{
    if (operations_.empty())
        return;

    for (auto p = operations_.begin(); p != operations_.end(); ++p) {
        epoll_event ev {};
        /* This is an optimization; We store a ptr to the first list item
         * for each FD directly in the event registration. When the event
         * is notified, we have an immediate reference to the operation,
         * meaning we don't need to traverse the list to find it...
         */
        ev.data.ptr = &*p;

        auto next = p;
        while (next != operations_.end() && next->get_fd() == p->get_fd()) {
            if (next->is_read_operation()) {
                ev.events |= EPOLLIN;
            }
            else {
                ev.events |= EPOLLOUT;
            }
            next++;
        }

        if (auto const r =
                ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, p->get_fd(), &ev);
            r < 0) {
            if (errno != EEXIST ||
                ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, p->get_fd(), &ev) < 0)
                throw std::system_error { errno, std::system_category() };
        }
    }

    auto const r =
        ::epoll_wait(epoll_fd_, event_buffer_.data(), event_buffer_.size(), -1);

    if (r < 0)
        throw std::system_error { errno, std::system_category() };

    std::span events_notified { event_buffer_.data(),
                                static_cast<std::size_t>(r) };

    for (auto& event : events_notified) {

        EXIOS_EXPECT(event.data.ptr);

        auto* op = reinterpret_cast<AsyncIoOperation*>(event.data.ptr);
        auto fd = op->get_fd();

        using IteratorType = decltype(operations_)::iterator;
        IteratorType next { op };

        bool pending = false;

        while (next != operations_.end() && next->get_fd() == fd) {

            /* We only want to perform IO on operations
             * matching the correct poll events received...
             */
            if (((event.events & EPOLLIN) != 0) !=
                (next->is_read_operation())) {
                pending = true;
                next++;
                continue;
            }

            auto io_result = next->perform_io();

            /* An EAGAIN / EWOULDBLOCK means we have to leave the
             * operation in the list. This may sound unecessary, but
             * we're allowing multiple operations to be queued for the
             * same FD, so only one IO operation per FD is guaranteed
             * to succeed without blocking...
             */
            if (io_result.is_error_value() &&
                (io_result.error() == std::errc::operation_would_block ||
                 io_result.error() ==
                     std::errc::resource_unavailable_try_again)) {
                pending = true;
                next++;
                continue;
            }

            next->set_result(io_result);

            /* WARNING: The order of these operations is crucial; We must
             * erase the item from `operations_` _BEFORE_ posting to the
             * context's completion queue, otherwise the call to `.erase()`
             * is operating on the completion queue's list instead of our
             * list - UB, basically!...
             */
            auto item = &*next;
            next = operations_.erase(next);
            item->get_context().post(item);
        }

        /* If we've processed all the pending operations for the
         * given FD then we can de-register it from epoll...
         */
        if (!pending) {
            if (auto const e =
                    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &event);
                e < 0) {
                throw std::system_error { errno, std::system_category() };
            }
        }
    }
}

} // namespace exios
