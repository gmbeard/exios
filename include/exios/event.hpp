#ifndef EXIOS_EVENT_HPP_INCLUDED
#define EXIOS_EVENT_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/context.hpp"
#include "exios/io.hpp"
#include "exios/io_object.hpp"
#include "exios/work.hpp"

namespace exios
{

/*!
 * An *I/O* object for triggering, and waiting on, events.
 *
 * Each triggered event will be consumed by, at most, one waiting consumer.
 *
 * ### Example
 *
 * ```cpp
 * exios::ContextThread thread;
 * exios::Event my_event { thread };
 *
 * my_event.wait_for_event([](exios::TimerOrEventIoResult result) {
 *   if (!result) {
 *     if (result.error() == std::errc::operation_canceled)
 *       return;
 *
 *     throw std::system_error { result.error() };
 *   }
 *
 *   std::cout << "Event received!\n";
 * });
 *
 * ...
 *
 * auto const num_completed = thread.run();
 * ```
 */
struct Event : IoObject
{
    Event(Context const& ctx);

    auto cancel() noexcept -> void;

    template <typename F>
    auto trigger(F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op =
            make_async_io_operation(event_write_operation,
                                    wrap_work(std::move(completion), ctx_),
                                    alloc,
                                    ctx_,
                                    fd_.value());

        schedule_io(op);
    }

    template <typename F>
    auto wait_for_event(F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op =
            make_async_io_operation(event_read_operation,
                                    wrap_work(std::move(completion), ctx_),
                                    alloc,
                                    ctx_,
                                    fd_.value());

        schedule_io(op);
    }
};

} // namespace exios

#endif // EXIOS_EVENT_HPP_INCLUDED
