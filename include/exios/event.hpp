#ifndef EXIOS_EVENT_HPP_INCLUDED
#define EXIOS_EVENT_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/context.hpp"
#include "exios/io.hpp"
#include "exios/io_object.hpp"
#include "exios/work.hpp"

namespace exios
{

struct Event : IoObject
{
    Event(Context const& ctx);

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
