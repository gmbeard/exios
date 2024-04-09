#include "exios/io_scheduler.hpp"
#include "exios/async_io_operation.hpp"
#include "exios/context_thread.hpp"
#include "exios/contracts.hpp"
#include "exios/intrusive_list.hpp"
#include "exios/poll_wake_event.hpp"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <errno.h>
#include <span>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>

namespace
{
constexpr std::size_t kMaxEventsPerPoll = 1024;

std::atomic_uint64_t threads_waiting = 0;

auto decrement_count(std::atomic_size_t& counter, std::size_t val) -> void
{
    auto const prev = counter.fetch_sub(val);
    EXIOS_EXPECT(val == 0 || prev >= val);
}

auto event_buffer() -> std::span<epoll_event>
{
    static thread_local std::vector<epoll_event> buffer(kMaxEventsPerPoll);
    return { buffer.data(), buffer.size() };
}

[[nodiscard]] auto process_notifications(
    int efd,
    std::span<epoll_event> events,
    exios::PollWakeEvent& wake_event,
    exios::IntrusiveList<exios::AsyncIoOperation>::iterator /*first*/,
    exios::IntrusiveList<exios::AsyncIoOperation>::iterator last,
    exios::IntrusiveList<exios::AsyncIoOperation>& list) noexcept -> std::size_t
{
    std::size_t total_processed = 0;

    for (auto& event : events) {

        auto fd = event.data.fd;
        if (fd == wake_event.get_fd()) {
            wake_event.reset();
            continue;
        }

        auto next = std::lower_bound(
            list.begin(), last, fd, [](auto const& item, auto const& val) {
                return item.get_fd() < val;
            });

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
            ::epoll_ctl(efd, EPOLL_CTL_DEL, fd, &event);
        }

        total_processed += items_processed_for_this_fd;
    }

    return total_processed;
}

auto process_cancellations(
    exios::IntrusiveList<exios::AsyncIoOperation>::iterator first,
    exios::IntrusiveList<exios::AsyncIoOperation>::iterator last,
    exios::IntrusiveList<exios::AsyncIoOperation>& list,
    std::size_t& count) noexcept
    -> exios::IntrusiveList<exios::AsyncIoOperation>::iterator
{
    auto next = first;
    while (next != last) {
        auto& item = *next;
        next = list.erase(next);
        ++count;
        item.get_context().post(&item);
    }

    return next;
}

} // namespace

namespace exios
{
IoScheduler::IoScheduler(ContextThread& ctx)
    : ctx_ { ctx }
    , epoll_fd_ { epoll_create1(EPOLL_CLOEXEC) }
    , begin_cancelled_ { operations_.end() }
{
    if (epoll_fd_ < 0)
        throw std::system_error { errno, std::system_category() };

    epoll_event ev {};
    ev.data.fd = wake_event_.get_fd();
    ev.events = EPOLLIN;
    if (auto const r =
            ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wake_event_.get_fd(), &ev);
        r < 0) {
        if (errno != EEXIST ||
            ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, wake_event_.get_fd(), &ev) <
                0) {
            throw std::system_error { errno, std::system_category() };
        }
    }
}

IoScheduler::~IoScheduler()
{
    ::close(epoll_fd_);
    drain_list(operations_, [](auto&& item) { discard(std::move(item)); });
}

auto IoScheduler::wake() noexcept -> void
{
    wake_event_.trigger(threads_waiting);
}

auto IoScheduler::schedule(AsyncIoOperation* op) noexcept -> void
{
    /* Scheduled operations are stored in a flat hash-table; They
     * are stored in order of their FD, with ops for the same FD
     * stored consecutively. This makes lookup operations a binary
     * search + N AsyncIoOperation per FD.
     */

    std::lock_guard lock { data_mutex_ };

    auto first = operations_.begin();
    auto last = begin_cancelled_;

    auto const fd = op->get_fd();
    epoll_event ev {};
    ev.data.fd = fd;
    ev.events = (op->is_read_operation() ? EPOLLIN : EPOLLOUT);

    auto insert_pos = last;

    if (first != last && fd <= (*first).get_fd()) {
        insert_pos = first;
    }
    else if (first != last && fd >= (*std::prev(last)).get_fd()) {
        insert_pos = last;
    }
    else {
        insert_pos = std::lower_bound(
            first, last, fd, [](auto const& a, auto const val) {
                return a.get_fd() < val;
            });
    }

    while (insert_pos != last && insert_pos->get_fd() == fd) {
        ev.events |= (insert_pos->is_read_operation() ? EPOLLIN : EPOLLOUT);
        ++insert_pos;
    }

    if (auto const r = ::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev); r < 0) {
        if (errno != EEXIST ||
            ::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
            // cppcheck-suppress [incorrectStringBooleanError]
            EXIOS_EXPECT(false && "Failed to register epoll event");
        }
    }

    static_cast<void>(operations_.insert(op, insert_pos));
    poll_queue_length_ += 1;
    ctx_.notify();
}

auto IoScheduler::cancel(int fd) noexcept -> void
{
    std::lock_guard lock { data_mutex_ };

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
    wake_event_.trigger(threads_waiting);
    ctx_.notify();
}

auto IoScheduler::empty() const noexcept -> bool
{
    return poll_queue_length_ == 0;
}

auto IoScheduler::poll_once() -> std::size_t
{
    {
        std::lock_guard lock { data_mutex_ };
        std::size_t num_cancelled = 0;
        begin_cancelled_ = process_cancellations(
            begin_cancelled_, operations_.end(), operations_, num_cancelled);

        EXIOS_EXPECT(begin_cancelled_ == operations_.end());
        decrement_count(poll_queue_length_, num_cancelled);

        if (operations_.begin() == begin_cancelled_) {
            EXIOS_EXPECT(poll_queue_length_ == 0);
            return 0;
        }
    }

    auto buffer = event_buffer();

    int poll_timeout = -1;
    std::size_t num_events = 0;
    std::size_t num_processed = 0;

    do {
        threads_waiting += 1;
        auto const r = ::epoll_pwait(
            epoll_fd_, buffer.data(), buffer.size(), poll_timeout, nullptr);
        threads_waiting -= 1;

        /* If we have to poll again, make sure we don't block...
         */
        poll_timeout = 0;

        if (r < 0)
            throw std::system_error { errno, std::system_category() };

        num_events = static_cast<std::size_t>(r);

        std::span events_notified { buffer.data(),
                                    static_cast<std::size_t>(r) };

        {
            std::lock_guard lock { data_mutex_ };
            auto const num = process_notifications(epoll_fd_,
                                                   events_notified,
                                                   wake_event_,
                                                   operations_.begin(),
                                                   begin_cancelled_,
                                                   operations_);

            decrement_count(poll_queue_length_, num);

            num_processed += num;
        }
    }
    while (num_events == buffer.size());

    return num_processed;
}

} // namespace exios
