#include "exios/io_object.hpp"
#include "exios/file_descriptor.hpp"
#include "exios/io_scheduler.hpp"

namespace exios
{

IoObject::IoObject(Context const& ctx, FileDescriptor&& fd) noexcept
    : ctx_ { ctx }
    , fd_ { std::move(fd) }
{
}

auto IoObject::schedule_io(AsyncIoOperation* op) noexcept -> void
{
    ctx_.io_scheduler().schedule(op);
}

auto schedule_io(Context ctx, AsyncIoOperation* op) noexcept -> void
{
    ctx.io_scheduler().schedule(op);
}

auto IoObject::get_context() const noexcept -> Context const&
{
    return ctx_;
}

auto IoObject::close() noexcept -> void
{
    ::close(fd_.value());
    fd_ = FileDescriptor {};
}

auto IoObject::cancel() noexcept -> void
{
    ctx_.io_scheduler().cancel(fd_.value());
}

} // namespace exios
