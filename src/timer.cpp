#include "exios/timer.hpp"
#include "exios/io_scheduler.hpp"
#include <bits/types/struct_itimerspec.h>
#include <errno.h>
#include <sys/timerfd.h>
#include <system_error>

namespace
{
constexpr auto kNanosecondsPerSecond = 1'000'000'000;
}

namespace exios
{

Timer::Timer(Context const& ctx)
    : IoObject { ctx,
                 ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC) }
{
    if (fd_.value() < 0)
        throw std::system_error { errno, std::system_category() };
}

auto Timer::expire() -> void
{
    auto timerval = convert_to_itimerspec(std::chrono::nanoseconds(1));

    if (auto const r = timerfd_settime(fd_.value(), 0, &timerval, nullptr);
        r < 0) {
        throw std::system_error { errno, std::system_category() };
    }
}

auto convert_to_itimerspec(std::chrono::nanoseconds const& val) -> itimerspec
{
    itimerspec retval {};

    retval.it_value.tv_sec = val.count() / kNanosecondsPerSecond;
    retval.it_value.tv_nsec = val.count() % kNanosecondsPerSecond;

    return retval;
}

} // namespace exios
