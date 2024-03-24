#ifndef EXIOS_IO_SCHEDULER_HPP_INCLUDED
#define EXIOS_IO_SCHEDULER_HPP_INCLUDED

#include "exios/async_io_operation.hpp"
#include "exios/intrusive_list.hpp"
#include <sys/epoll.h>
#include <vector>

namespace exios
{

struct IoScheduler
{
    IoScheduler();
    ~IoScheduler();

    IoScheduler(IoScheduler const&) = delete;
    auto operator=(IoScheduler const&) -> IoScheduler& = delete;

    auto schedule(AsyncIoOperation* op) noexcept -> void;
    auto poll_once() -> void;

private:
    int epoll_fd_;
    IntrusiveList<AsyncIoOperation> operations_;
    std::vector<epoll_event> event_buffer_;
};

} // namespace exios

#endif // EXIOS_IO_SCHEDULER_HPP_INCLUDED
