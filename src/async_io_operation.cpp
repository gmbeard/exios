#include "exios/async_io_operation.hpp"

namespace exios
{
AsyncIoOperation::AsyncIoOperation(Context ctx,
                                   int fd,
                                   bool is_read_operation) noexcept
    : ctx_ { ctx }
    , fd_ { fd }
    , is_read_ { is_read_operation }
    , is_cancelled_ { false }
{
}

auto AsyncIoOperation::cancelled() const noexcept -> bool
{
    return is_cancelled_;
}

auto AsyncIoOperation::cancel() noexcept -> void
{
    do_cancel();
    is_cancelled_ = true;
}

auto AsyncIoOperation::get_context() noexcept -> Context& { return ctx_; }

auto AsyncIoOperation::get_fd() const noexcept -> int { return fd_; }

auto AsyncIoOperation::is_read_operation() const noexcept -> bool
{
    return is_read_;
}

} // namespace exios
