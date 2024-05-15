#include "exios/event.hpp"
#include "exios/buffer_view.hpp"
#include "exios/io.hpp"
#include "exios/io_scheduler.hpp"
#include <cinttypes>
#include <errno.h>
#include <sys/eventfd.h>
#include <system_error>

namespace exios
{

Event::Event(Context const& ctx)
    : IoObject { ctx, ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK) }
{
    if (fd_.value() < 0)
        throw std::system_error { errno, std::system_category() };
}

auto Event::cancel() noexcept -> void
{
    ctx_.io_scheduler().cancel(fd_.value());
}

} // namespace exios
