#include "exios/context.hpp"
#include "exios/async_operation.hpp"
#include "exios/context_thread.hpp"
#include <memory>

namespace exios
{

Context::Context(ContextThread& thread) noexcept
    : thread_ { std::addressof(thread) }
{
}

auto Context::post(AnyAsyncOperation* op) -> void { thread_->post(op); }

auto Context::latch_work() noexcept -> void { thread_->latch_work(); }

auto Context::release_work() noexcept -> void { thread_->release_work(); }

auto Context::io_scheduler() noexcept -> IoScheduler&
{
    return thread_->io_scheduler();
}

} // namespace exios
