#ifndef EXIOS_TIMER_HPP_INCLUDED
#define EXIOS_TIMER_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/async_io_operation.hpp"
#include "exios/buffer_view.hpp"
#include "exios/context.hpp"
#include "exios/file_descriptor.hpp"
#include "exios/io.hpp"
#include "exios/io_object.hpp"
#include "exios/work.hpp"
#include <bits/types/struct_itimerspec.h>
#include <chrono>
#include <cinttypes>
#include <errno.h>
#include <memory>
#include <sys/timerfd.h>

namespace exios
{

auto convert_to_itimerspec(std::chrono::nanoseconds const& val) -> itimerspec;

struct Timer : IoObject
{
    Timer(Context const& ctx);

    auto expire() -> void;
    auto cancel() -> void;

    template <typename Rep, typename Period, typename F>
    auto wait_for_expiry_after(std::chrono::duration<Rep, Period> duration,
                               F&& completion) -> void
    {
        auto timerval = convert_to_itimerspec(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration));

        if (auto const r = timerfd_settime(fd_.value(), 0, &timerval, nullptr);
            r < 0) {
            throw std::system_error { errno, std::system_category() };
        }

        auto const alloc = select_allocator(completion);

        auto* op =
            make_async_io_operation(timer_expiry_operation,
                                    wrap_work(std::move(completion), ctx_),
                                    alloc,
                                    ctx_,
                                    fd_.value());

        schedule_io(op);
    }
};

} // namespace exios

#endif // EXIOS_TIMER_HPP_INCLUDED
