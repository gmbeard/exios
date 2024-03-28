#ifndef EXIOS_IO_OBJECT_HPP_INCLUDED
#define EXIOS_IO_OBJECT_HPP_INCLUDED

#include "exios/async_io_operation.hpp"
#include "exios/context.hpp"
#include "exios/file_descriptor.hpp"
#include "exios/work.hpp"

namespace exios
{

struct IoObject
{
    IoObject(Context const&, FileDescriptor&& fd) noexcept;

protected:
    auto schedule_io(AsyncIoOperation* op) noexcept -> void;

    template <typename F, typename Alloc>
    auto post_completion(F&& f, Alloc const& alloc)
    {
        ctx_.post(wrap_work(std::forward<F>(f), ctx_), alloc);
    }

    Context ctx_;
    FileDescriptor fd_;
};

} // namespace exios

#endif // EXIOS_EVENT_HPP_INCLUDED
