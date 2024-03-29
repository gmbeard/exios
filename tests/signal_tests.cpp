#include "exios/exios.hpp"
#include "testing.hpp"
#include "tracking_allocator.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

template <typename F>
struct WaitForSignalWithTimeout
{
    WaitForSignalWithTimeout(exios::Signal& sig, exios::Timer& tmr, F&& f)
        : signal { sig }
        , timer { tmr }
        , completion { std::move(f) }
    {
    }

    template <typename T, typename U>
    auto operator()(std::chrono::duration<T, U> timeout) -> void
    {
        struct SharedState
        {
            exios::Signal& signal;
            exios::Timer& timer;
            F f;
            int pending;
            std::optional<exios::SignalResult> result { std::nullopt };
        };

        auto alloc = exios::select_allocator(completion);

        auto shared_state = std::allocate_shared<SharedState>(
            alloc, signal, timer, std::move(completion), 0);

        signal.wait(exios::use_allocator(
            [shared_state](exios::SignalResult result) mutable {
                shared_state->timer.cancel();
                if (--shared_state->pending) {
                    shared_state->result.emplace(std::move(result));
                    shared_state.reset();
                }
                else {
                    EXIOS_EXPECT(shared_state->result &&
                                 shared_state->result->is_error_value());
                    auto e = std::move(shared_state->result->error());
                    auto c = std::move(shared_state->f);
                    shared_state.reset();
                    c(exios::result_error(e));
                }
            },
            alloc));

        timer.wait_for_expiry_after(
            timeout,
            exios::use_allocator(
                [shared_state](exios::TimerOrEventIoResult) mutable {
                    shared_state->signal.cancel();
                    if (--shared_state->pending) {
                        shared_state->result.emplace(exios::result_error(
                            std::make_error_code(std::errc::timed_out)));
                        shared_state.reset();
                    }
                    else {
                        EXIOS_EXPECT(shared_state->result);
                        auto r = std::move(*shared_state->result);
                        auto c = std::move(shared_state->f);
                        shared_state.reset();
                        c(r);
                    }
                },
                alloc));
    }

    exios::Signal& signal;
    exios::Timer& timer;
    F completion;
};

template <typename T, typename U, typename F>
auto wait_for_signal_with_timeout(exios::Signal& signal,
                                  exios::Timer& timer,
                                  std::chrono::duration<T, U> timeout,
                                  F&& f) -> void
{
    WaitForSignalWithTimeout op { signal, timer, std::forward<F>(f) };
    op(timeout);
}

auto should_get_signal() -> void
{
    exios::ContextThread thread;
    exios::Signal signal { thread, SIGINT };
    exios::Timer timer { thread };

    std::intptr_t allocations = 0;

    auto alloc_cb = [&](auto allocating, auto) {
        if (allocating) {
            allocations += 1;
        }
        else {
            allocations -= 1;
        }
    };

    wait_for_signal_with_timeout(signal,
                                 timer,
                                 std::chrono::milliseconds(10),
                                 exios::use_allocator(
                                     [&](exios::SignalResult result) {
                                         EXPECT(result.is_error_value() &&
                                                result.error() ==
                                                    std::errc::timed_out);
                                         EXPECT(allocations == 0);
                                     },
                                     tracking_allocator<void>(alloc_cb)));

    static_cast<void>(thread.run());
}

template <typename T, typename U>
auto sleep_for(std::chrono::duration<T, U> duration) -> void
{
    auto const micros =
        std::chrono::duration_cast<std::chrono::microseconds>(duration);

    timeval dur = { .tv_sec = micros.count() / 1'000'000,
                    .tv_usec = micros.count() % 1'000'000 };

    EXPECT(::select(0, nullptr, nullptr, nullptr, &dur) == 0);
}

auto should_handle_sigint() -> void
{
    std::array<int, 2> std_err_pipe;
    EXPECT(::pipe(std_err_pipe.data()) >= 0);

    auto const pid = ::fork();
    EXPECT(pid >= 0);

    if (pid == 0) {

        sigset_t blocked;
        sigemptyset(&blocked);
        sigaddset(&blocked, SIGINT);
        pthread_sigmask(SIG_BLOCK, &blocked, nullptr);

        EXPECT(::dup2(std_err_pipe[1], STDERR_FILENO) >= 0);
        ::close(std_err_pipe[0]);
        ::close(std_err_pipe[1]);

        exios::ContextThread thread;
        exios::Signal signal { thread, SIGINT };
        signal.wait([](exios::SignalResult const& result) {
            if (result)
                ::dprintf(STDERR_FILENO, "Success");
            else
                ::dprintf(
                    STDERR_FILENO, "%s", result.error().message().c_str());
        });

        static_cast<void>(thread.run());
        ::fsync(STDERR_FILENO);
    }
    else {
        ::close(std_err_pipe[1]);

        /* We need to give the child process chance to
         * set up its event loop...
         */
        sleep_for(std::chrono::milliseconds(200));

        EXPECT(::kill(pid, SIGINT) >= 0);

        std::array<char, 128> buffer;
        std::string child_stderr;
        while (true) {
            auto const n =
                ::read(std_err_pipe[0], buffer.data(), buffer.size());
            EXPECT(n >= 0);
            if (n == 0)
                break;

            std::copy_n(buffer.begin(),
                        static_cast<std::size_t>(n),
                        std::back_inserter(child_stderr));
        }

        int status;
        EXPECT(::waitpid(pid, &status, 0) >= 0);
        EXPECT(WEXITSTATUS(status) == 0);

        std::cerr << child_stderr << '\n';
        EXPECT(child_stderr.starts_with("Success"));
    }
}

auto main() -> int
{
    return testing::run(
        { TEST(should_get_signal), TEST(should_handle_sigint) });
}
