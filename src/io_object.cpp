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

} // namespace exios
