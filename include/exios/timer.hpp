#ifndef EXIOS_TIMER_HPP_INCLUDED
#define EXIOS_TIMER_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/async_io_operation.hpp"
#include "exios/buffer_view.hpp"
#include "exios/context.hpp"
#include "exios/io.hpp"
#include "exios/work.hpp"
#include <bits/types/struct_itimerspec.h>
#include <chrono>
#include <cinttypes>
#include <errno.h>
#include <memory>
#include <sys/timerfd.h>

namespace exios
{

struct FileDescriptor
{
    static constexpr int kInvalidDescriptor = -1;

    FileDescriptor(int) noexcept;
    FileDescriptor(FileDescriptor&&) noexcept;
    ~FileDescriptor();
    auto operator=(FileDescriptor&&) noexcept -> FileDescriptor&;
    auto value() const noexcept -> int const&;
    friend auto swap(FileDescriptor&, FileDescriptor&) noexcept -> void;

private:
    int fd_;
};

auto convert_to_itimerspec(std::chrono::nanoseconds const& val) -> itimerspec;

struct Timer
{
    Timer(Context const& ctx);

    auto expire() -> void;

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

private:
    auto schedule_io(AsyncIoOperation* op) noexcept -> void;

    Context ctx_;
    FileDescriptor fd_;
};

} // namespace exios

#endif // EXIOS_TIMER_HPP_INCLUDED
