#include "exios/context_thread.hpp"
#include "exios/async_operation.hpp"
#include "exios/contracts.hpp"
#include "exios/intrusive_list.hpp"
#include "exios/scope_guard.hpp"
#include <cstddef>
#include <limits>

namespace exios
{

ContextThread::ContextThread()
    : poll_sentinel_ { make_async_operation([] {}, std::allocator<void> {}) }
{
}

ContextThread::~ContextThread()
{
    drain_list(completion_queue_, [&](auto&& item) noexcept {
        /* Ignore the poll sentinel. This will be cleaned up automatically
         */
        if (std::addressof(item) != poll_sentinel_.get()) {
            discard(std::move(item));
        }
    });
}

auto ContextThread::latch_work() noexcept -> void
{
    [[maybe_unused]] auto const prev = remaining_count_.fetch_add(1);
    EXIOS_EXPECT(prev < std::numeric_limits<decltype(prev)>::max());
}

auto ContextThread::release_work() noexcept -> void
{
    [[maybe_unused]] auto const prev = remaining_count_.fetch_sub(1);
    EXIOS_EXPECT(prev > 0);
}

auto ContextThread::post(AnyAsyncOperation* op) noexcept -> void
{
    std::lock_guard lock { data_mutex_ };
    completion_queue_.push_back(op);
}

auto ContextThread::run_once() -> std::size_t
{
    IntrusiveList<AnyAsyncOperation> tmp;

    {
        std::lock_guard lock { data_mutex_ };
        static_cast<void>(tmp.splice(tmp.end(), completion_queue_));
    }

    EXIOS_SCOPE_GUARD([&] {
        std::lock_guard lock { data_mutex_ };
        completion_queue_.push_back(poll_sentinel_.get());
    });

    EXIOS_SCOPE_GUARD([&] {
        if (!tmp.empty()) {
            /* If an exception is thrown then we must put all
             * unprocessed completions back onto the queue...
             */
            std::lock_guard lock { data_mutex_ };
            static_cast<void>(
                completion_queue_.splice(completion_queue_.begin(), tmp));
        }
    });

    std::size_t num_processed = 0;

    drain_list(tmp, [&](auto&& item) {
        if (std::addressof(item) == poll_sentinel_.get()) {
            static_cast<void>(io_scheduler_.poll_once());
        }
        else {
            dispatch(std::move(item));
            num_processed += 1;
        }
    });

    return num_processed;
}

auto ContextThread::run() -> std::size_t
{
    std::size_t num_processed = 0;
    while (remaining_count_ > 0) {
        num_processed += run_once();
    }

    return num_processed;
}

auto ContextThread::io_scheduler() noexcept -> IoScheduler&
{
    return io_scheduler_;
}

auto ContextThread::AnyAsyncOperationDeleter::operator()(
    AnyAsyncOperation* ptr) noexcept -> void
{
    discard(std::move(*ptr));
}

} // namespace exios
