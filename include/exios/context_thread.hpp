#ifndef EXIOS_CONTEXT_THREAD_HPP_INCLUDED
#define EXIOS_CONTEXT_THREAD_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/async_operation.hpp"
#include "exios/intrusive_list.hpp"
#include "exios/io_scheduler.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace exios
{

struct ContextThread
{
    friend struct IoScheduler;

    ContextThread() noexcept;
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
        post(make_async_operation(wrap_work(std::forward<F>(f), *this), alloc));
    }

    template <typename F>
    auto post(F&& f) -> void
    requires(!std::is_convertible_v<F, AnyAsyncOperation*>)
    {
        auto const alloc = select_allocator(f);
        post(std::forward<F>(f), alloc);
    }

    auto io_scheduler() noexcept -> IoScheduler&;

    auto get_context() const noexcept -> Context;

private:
    auto notify() noexcept -> void;

    IoScheduler io_scheduler_;
    IntrusiveList<AnyAsyncOperation> completion_queue_;
    std::atomic_size_t remaining_count_ { 0 };
    std::mutex data_mutex_;
    std::condition_variable cvar_;
};

} // namespace exios

#endif // EXIOS_CONTEXT_THREAD_HPP_INCLUDED
