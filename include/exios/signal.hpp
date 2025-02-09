#ifndef EXIOS_SIGNAL_HPP_INCLUDED
#define EXIOS_SIGNAL_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/async_io_operation.hpp"
#include "exios/io.hpp"
#include "exios/io_object.hpp"

namespace exios
{

struct Signal : IoObject
{
    Signal(Context const& ctx, int sig);

    template <typename F>
    auto wait(F&& f) -> void
    {
        auto alloc = select_allocator(f);
        auto* op = make_async_io_operation(signal_read_operation,
                                           wrap_work(std::forward<F>(f), ctx_),
                                           alloc,
                                           ctx_,
                                           fd_.value());

        schedule_io(op);
    }
};

} // namespace exios

#endif // EXIOS_SIGNAL_HPP_INCLUDED
