#include "exios/signal.hpp"
#include "exios/io_scheduler.hpp"
#include <errno.h>
#include <signal.h>
#include <sys/signalfd.h>

namespace exios
{

auto Signal::cancel() -> void { ctx_.io_scheduler().cancel(fd_.value()); }

Signal::Signal(Context const& ctx, int sig)
    : IoObject { ctx, [&] {
                    sigset_t mask;
                    if (sigemptyset(&mask) < 0)
                        return -1;
                    if (sigaddset(&mask, sig) < 0)
                        return -1;
                    return ::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
                }() }
{
    if (fd_.value() < 0)
        throw std::system_error { errno, std::system_category() };
}

} // namespace exios
