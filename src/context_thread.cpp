#include "exios/context_thread.hpp"
#include "exios/async_operation.hpp"
#include "exios/contracts.hpp"
#include "exios/intrusive_list.hpp"
#include "exios/scope_guard.hpp"
#include <cstddef>
#include <limits>

namespace
{

struct SentinelAsyncOperation final : exios::AnyAsyncOperation
{
    auto dispatch() -> void override {}
    auto discard() noexcept -> void override {}
};

auto poll_sentinel() -> exios::AnyAsyncOperation*
{
    thread_local SentinelAsyncOperation sentinel;
    return &sentinel;
}

} // namespace

namespace exios
{

ContextThread::ContextThread() noexcept
    : io_scheduler_(*this)
{
}

ContextThread::~ContextThread()
{
    std::lock_guard lock { data_mutex_ };
    drain_list(completion_queue_, [&](auto&& item) noexcept {
        /* Ignore the poll sentinel. This will be cleaned up automatically
         */
        if (std::addressof(item) != poll_sentinel()) {
            discard(std::move(item));
        }
    });
}

auto ContextThread::latch_work() noexcept -> void
{
    auto const prev = remaining_count_.fetch_add(1);
    EXIOS_EXPECT(prev < std::numeric_limits<decltype(prev)>::max());
}

auto ContextThread::release_work() noexcept -> void
{
    auto const prev = remaining_count_.fetch_sub(1);
    EXIOS_EXPECT(prev > 0);
    io_scheduler_.wake();
    cvar_.notify_all();
}

auto ContextThread::post(AnyAsyncOperation* op) noexcept -> void
{
    std::lock_guard lock { data_mutex_ };
    completion_queue_.push_back(op);
    EXIOS_EXPECT(!completion_queue_.empty());
    io_scheduler_.wake();
    cvar_.notify_all();
}

auto ContextThread::run_once() -> std::size_t
{
    IntrusiveList<AnyAsyncOperation> tmp;

    {
        std::lock_guard lock { data_mutex_ };
        static_cast<void>(tmp.splice(tmp.end(), completion_queue_));
        EXIOS_EXPECT(completion_queue_.empty());
    }

    EXIOS_SCOPE_GUARD([this] { cvar_.notify_all(); });

    EXIOS_SCOPE_GUARD([&] {
        if (!tmp.empty()) {
            std::lock_guard lock { data_mutex_ };
            /* If an exception is thrown then we must put all
             * unprocessed completions back onto the queue...
             */
            static_cast<void>(
                completion_queue_.splice(completion_queue_.begin(), tmp));
        }
    });

    std::size_t num_processed = 0;

    drain_list(tmp, [&](auto&& item) {
        dispatch(std::move(item));
        num_processed += 1;
    });

    if (!io_scheduler_.empty())
        static_cast<void>(io_scheduler_.poll_once());

    return num_processed;
}

auto ContextThread::notify() noexcept -> void { cvar_.notify_all(); }

auto ContextThread::run() -> std::size_t
{
    std::size_t num_processed = 0;
    while (remaining_count_ > 0) {
        {
            std::unique_lock lock { data_mutex_ };
            if (completion_queue_.empty() && io_scheduler_.empty() &&
                remaining_count_ > 0) {
                cvar_.wait(lock, [this] {
                    return !completion_queue_.empty() ||
                           !io_scheduler_.empty() || remaining_count_ == 0;
                });
            }
        }

        num_processed += run_once();
    }

    io_scheduler_.wake();
    cvar_.notify_all();

    return num_processed;
}

auto ContextThread::io_scheduler() noexcept -> IoScheduler&
{
    return io_scheduler_;
}

} // namespace exios
