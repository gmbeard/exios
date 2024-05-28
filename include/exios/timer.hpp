#ifndef EXIOS_TIMER_HPP_INCLUDED
#define EXIOS_TIMER_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/async_io_operation.hpp"
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

template <typename Rep, typename Period, typename F>
auto wait_for_timer_expiry_after(Context const& ctx,
                                 std::chrono::duration<Rep, Period> duration,
                                 F&& completion) -> void;

struct Timer : IoObject
{
    Timer(Context const& ctx);

    auto expire() -> void;
    auto cancel() -> void;

    template <typename Rep, typename Period, typename F>
    auto wait_for_expiry_after(std::chrono::duration<Rep, Period> duration,
                               F&& completion) -> void
    {
        cancel();
        if (duration == std::chrono::nanoseconds::zero()) {
            auto const alloc = select_allocator(completion);
            auto f = [completion = std::move(completion)]() mutable {
                std::move(completion)(TimerOrEventIoResult { result_ok(0ull) });
            };

            ctx_.post(std::move(f), alloc);
            return;
        }

        auto timerval = convert_to_itimerspec(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration));

        if (auto const r = timerfd_settime(fd_.value(), 0, &timerval, nullptr);
            r < 0) {
            throw std::system_error { errno, std::system_category() };
        }

        queue_timer_operation(ctx_, fd_.value(), std::forward<F>(completion));
    }

    template <typename Rep, typename Period, typename F>
    friend auto
    wait_for_timer_expiry_after(Context const& ctx,
                                std::chrono::duration<Rep, Period> duration,
                                F&& completion) -> void
    {
        Timer t { ctx };
        auto fd = std::move(t.fd_);
        auto timerval = convert_to_itimerspec(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration));

        if (auto const r = timerfd_settime(fd.value(), 0, &timerval, nullptr);
            r < 0) {
            throw std::system_error { errno, std::system_category() };
        }

        int fd_val = fd.value();

        Timer::queue_timer_operation(
            ctx,
            fd_val,
            [fd = std::move(fd), f = std::move(completion)](
                auto&& result) mutable { std::move(f)(std::move(result)); });
    }

private:
    template <typename F>
    static auto
    queue_timer_operation(Context const& ctx, int fd, F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op =
            make_async_io_operation(timer_expiry_operation,
                                    wrap_work(std::move(completion), ctx),
                                    alloc,
                                    ctx,
                                    fd);

        exios::schedule_io(ctx, op);
    }
};

} // namespace exios

#endif // EXIOS_TIMER_HPP_INCLUDED
