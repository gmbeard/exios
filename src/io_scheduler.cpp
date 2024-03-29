#include "exios/io_scheduler.hpp"
#include "exios/async_io_operation.hpp"
#include "exios/contracts.hpp"
#include "exios/intrusive_list.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <errno.h>
#include <span>
#include <sys/epoll.h>
#include <unistd.h>

namespace
{
constexpr std::size_t kMaxEventsPerPoll = 1024;

[[nodiscard]] auto
register_events(int efd,
                exios::IntrusiveList<exios::AsyncIoOperation>::iterator first,
                exios::IntrusiveList<exios::AsyncIoOperation>::iterator last)
    -> std::size_t
{
    std::size_t num = 0;

    auto p = first;
    while (p != last) {
        auto const fd = p->get_fd();
        epoll_event ev {};
        /* This is an optimization; We store a ptr to the first list item
         * for each FD directly in the event registration. When the event
         * is notified, we have an immediate reference to the operation,
         * meaning we don't need to traverse the list to find it...
         */
        ev.data.ptr = &*p;

        while (p != last && p->get_fd() == fd) {
            if (p->is_read_operation()) {
                ev.events |= EPOLLIN;
            }
            else {
                ev.events |= EPOLLOUT;
            }
            ++p;
        }

        if (auto const r = ::epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev); r < 0) {
            if (errno != EEXIST || ::epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev) < 0)
                throw std::system_error { errno, std::system_category() };
        }
        num += 1;
    }

    return num;
}

[[nodiscard]] auto process_notifications(
    int efd,
    std::span<epoll_event> events,
    exios::IntrusiveList<exios::AsyncIoOperation>::iterator first,
    exios::IntrusiveList<exios::AsyncIoOperation>::iterator last,
    exios::IntrusiveList<exios::AsyncIoOperation>& list) -> std::size_t
{
    std::size_t total_processed = 0;

    for (auto& event : events) {

        EXIOS_EXPECT(event.data.ptr);

        auto* op = reinterpret_cast<exios::AsyncIoOperation*>(event.data.ptr);
        auto fd = op->get_fd();

        using IteratorType = decltype(first);
        IteratorType next { op };

        std::size_t items_for_this_fd = 0;
        std::size_t items_processed_for_this_fd = 0;

        while (next != last && next->get_fd() == fd) {

            items_for_this_fd++;
            auto& item = *next++;

            if (item.cancelled()) {
                continue;
            }

            /* We only want to perform IO on operations
             * matching the correct poll events received...
             */
            if (((event.events & EPOLLIN) != 0) != (item.is_read_operation())) {
                continue;
            }

            /* A return value of `false` means the I/O operation
             * resulted in EAGAIN / EWOULDBLOCK, and we have to leave the
             * operation in the list. This may sound unecessary, but
             * we're allowing multiple operations to be queued for the
             * same FD, so only one IO operation per FD is guaranteed
             * to succeed without blocking...
             */
            if (!item.perform_io()) {
                continue;
            }

            /* WARNING: The order of these operations is crucial; We must
             * erase the item from `list` _BEFORE_ posting to the
             * context's completion queue, otherwise the call to `.erase()`
             * is operating on the completion queue's list instead of our
             * list - UB, basically!...
             */
            next = list.erase(&item);
            item.get_context().post(&item);

            items_processed_for_this_fd++;
        }

        /* If we've processed all the pending operations for the
         * given FD then we can de-register it from epoll...
         */
        if (items_for_this_fd == items_processed_for_this_fd) {
            if (auto const e = ::epoll_ctl(efd, EPOLL_CTL_DEL, fd, &event);
                e < 0) {
                throw std::system_error { errno, std::system_category() };
            }
        }

        total_processed += items_processed_for_this_fd;
    }

    return total_processed;
}

auto process_cancellations(
    exios::IntrusiveList<exios::AsyncIoOperation>::iterator first,
    exios::IntrusiveList<exios::AsyncIoOperation>::iterator last,
    exios::IntrusiveList<exios::AsyncIoOperation>& list)
    -> exios::IntrusiveList<exios::AsyncIoOperation>::iterator
{
    while (first != last) {
        auto& item = *first;
        first = list.erase(first);
        item.get_context().post(&item);
    }

    return first;
}

} // namespace

namespace exios
{
IoScheduler::IoScheduler()
    : epoll_fd_ { epoll_create1(EPOLL_CLOEXEC) }
    , begin_cancelled_ { operations_.end() }
{
    if (epoll_fd_ < 0)
        throw std::system_error { errno, std::system_category() };

    event_buffer_.resize(kMaxEventsPerPoll);
}

IoScheduler::~IoScheduler()
{
    ::close(epoll_fd_);
    drain_list(operations_, [](auto&& item) { discard(std::move(item)); });
}

auto IoScheduler::schedule(AsyncIoOperation* op) noexcept -> void
{
    /* TODO: Thread safety?
     */

    /* Scheduled operations are stored in a flat hash-table; They
     * are stored in order of their FD, with ops for the same FD
     * stored consecutively. This makes lookup operations a binary
     * search + N AsyncIoOperation per FD.
     */

    auto first = operations_.begin();
    auto last = begin_cancelled_;

    auto const fd = op->get_fd();

    auto insert_pos =
        std::lower_bound(first, last, fd, [](auto const& a, auto const val) {
            return a.get_fd() < val;
        });

    while (insert_pos != last && insert_pos->get_fd() == fd)
        ++insert_pos;

    static_cast<void>(operations_.insert(op, insert_pos));
}

auto IoScheduler::cancel(int fd) noexcept -> void
{
    /* TODO: What if this is called from a thread other
     * than the one that's polling??
     */

    auto first = operations_.begin();
    auto last = begin_cancelled_;

    /* Find the range where all items equal `fd`...
     */
    auto first_pos =
        std::lower_bound(first, last, fd, [](auto const& a, auto const val) {
            return a.get_fd() < val;
        });

    auto last_pos = std::find_if(
        first_pos, last, [&](auto const& item) { return item.get_fd() != fd; });

    /* We didn't find any FDs to cancel...
     */
    if (first_pos == last_pos)
        return;

    /* Move the cancelled FDs to the back of the list. We won't
     * remove them in anticipation of `cancel` being called from
     * a different thread; The FD may get notified while we're
     * cancelling it, so it needs to remain at least somewhere
     * in the list until we can safely remove it...
     */
    begin_cancelled_ =
        operations_.splice(begin_cancelled_, first_pos, last_pos);

    /* Cancel the operations and de-register the FD...
     */

    std::for_each(first_pos, last_pos, [](auto& item) { item.cancel(); });

    ::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
}

auto IoScheduler::poll_once() -> std::size_t
{
    begin_cancelled_ =
        process_cancellations(begin_cancelled_, operations_.end(), operations_);

    EXIOS_EXPECT(begin_cancelled_ == operations_.end());

    if (0 == register_events(epoll_fd_, operations_.begin(), begin_cancelled_))
        return 0;

    auto const r = ::epoll_pwait(
        epoll_fd_, event_buffer_.data(), event_buffer_.size(), -1, nullptr);

    if (r < 0)
        throw std::system_error { errno, std::system_category() };

    std::span events_notified { event_buffer_.data(),
                                static_cast<std::size_t>(r) };

    return process_notifications(epoll_fd_,
                                 events_notified,
                                 operations_.begin(),
                                 begin_cancelled_,
                                 operations_);
}

} // namespace exios
