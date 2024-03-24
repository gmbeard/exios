#ifndef EXIOS_CONTEXT_THREAD_HPP_INCLUDED
#define EXIOS_CONTEXT_THREAD_HPP_INCLUDED

#include "exios/async_operation.hpp"
#include "exios/intrusive_list.hpp"
#include "exios/io_scheduler.hpp"
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>

namespace exios
{

struct ContextThread
{
    ContextThread();
    ~ContextThread();
    ContextThread(ContextThread const&) = delete;
    auto operator=(ContextThread const&) -> ContextThread& = delete;

    auto latch_work() noexcept -> void;
    auto release_work() noexcept -> void;
    [[nodiscard]] auto run_once() -> std::size_t;
    [[nodiscard]] auto run() -> std::size_t;

    auto post(AnyAsyncOperation* op) noexcept -> void;

    template <typename F, typename Alloc>
    auto post(F&& f, Alloc const& alloc) -> void
    {
        post(make_async_operation(std::forward<F>(f), alloc));
    }

    auto io_scheduler() noexcept -> IoScheduler&;

private:
    struct AnyAsyncOperationDeleter
    {
        auto operator()(AnyAsyncOperation* ptr) noexcept -> void;
    };

    std::unique_ptr<AnyAsyncOperation, AnyAsyncOperationDeleter> poll_sentinel_;
    IoScheduler io_scheduler_;
    IntrusiveList<AnyAsyncOperation> completion_queue_;
    std::atomic_size_t remaining_count_ { 0 };
    std::mutex data_mutex_;
};

} // namespace exios

#endif // EXIOS_CONTEXT_THREAD_HPP_INCLUDED
