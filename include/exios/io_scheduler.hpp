#ifndef EXIOS_IO_SCHEDULER_HPP_INCLUDED
#define EXIOS_IO_SCHEDULER_HPP_INCLUDED

#include "exios/async_io_operation.hpp"
#include "exios/intrusive_list.hpp"
#include "exios/poll_wake_event.hpp"
#include <atomic>
#include <cstddef>
#include <mutex>

namespace exios
{

struct ContextThread;

struct IoScheduler
{
    IoScheduler(ContextThread&);
    ~IoScheduler();

    IoScheduler(IoScheduler const&) = delete;
    auto operator=(IoScheduler const&) -> IoScheduler& = delete;

    auto wake() noexcept -> void;
    auto empty() const noexcept -> bool;
    auto schedule(AsyncIoOperation* op) noexcept -> void;
    auto cancel(int fd) noexcept -> void;
    [[nodiscard]] auto poll_once(bool block = true) -> std::size_t;

private:
    ContextThread& ctx_;
    int epoll_fd_;
    PollWakeEvent wake_event_;
    IntrusiveList<AsyncIoOperation> operations_;
    IntrusiveList<AsyncIoOperation>::iterator begin_cancelled_;
    mutable std::mutex data_mutex_;
    std::atomic_size_t poll_queue_length_ { 0 };
};

} // namespace exios

#endif // EXIOS_IO_SCHEDULER_HPP_INCLUDED
