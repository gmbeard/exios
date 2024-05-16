#ifndef EXIOS_CONTEXT_HPP_INCLUDED
#define EXIOS_CONTEXT_HPP_INCLUDED

#include "exios/async_operation.hpp"
#include "exios/work.hpp"

namespace exios
{

struct ContextThread;
struct IoScheduler;

struct Context
{
    Context(ContextThread&) noexcept;
    auto post(AnyAsyncOperation* op) -> void;
    auto latch_work() noexcept -> void;
    auto release_work() noexcept -> void;
    auto io_scheduler() noexcept -> IoScheduler&;

    template <typename F, typename Alloc>
    auto post(F&& f, Alloc const& alloc) -> void
    {
        post(make_async_operation(wrap_work(std::forward<F>(f), *this), alloc));
    }

private:
    ContextThread* thread_;
};

} // namespace exios

#endif // EXIOS_CONTEXT_HPP_INCLUDED
