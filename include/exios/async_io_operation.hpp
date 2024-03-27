#ifndef EXIOS_ASYNC_IO_OPERATION_HPP_INCLUDED
#define EXIOS_ASYNC_IO_OPERATION_HPP_INCLUDED

#include "exios/async_operation.hpp"
#include "exios/buffer_view.hpp"
#include "exios/context.hpp"
#include "exios/io.hpp"
#include <system_error>
#include <type_traits>

namespace exios
{
struct AsyncIoOperation : AnyAsyncOperation
{
    using ResultType = std::size_t;
    using ErrorType = std::error_code;

    AsyncIoOperation(Context ctx, int fd, bool is_read_operation) noexcept;

    [[nodiscard]] virtual auto perform_io() noexcept -> bool = 0;
    auto cancel() noexcept -> void;
    [[nodiscard]] auto cancelled() const noexcept -> bool;
    [[nodiscard]] auto get_context() noexcept -> Context&;
    [[nodiscard]] auto get_fd() const noexcept -> int;
    [[nodiscard]] auto is_read_operation() const noexcept -> bool;

protected:
    virtual auto do_cancel() noexcept -> void = 0;
    std::optional<Result<ResultType, ErrorType>> result_;

private:
    Context ctx_;
    int fd_;
    bool is_read_;
    bool is_cancelled_;
};

template <typename OperationTag, typename F, typename Alloc>
struct AsyncIoOperationImpl : AsyncIoOperation
{
    using Operation = IoOperationType<OperationTag>;

    template <typename... Args>
    AsyncIoOperationImpl(
        F&& f, Alloc const& alloc, Context ctx, int fd, Args&&... args) noexcept
        : AsyncIoOperation { ctx, fd, Operation::is_readable }
        , f_ { std::move(f) }
        , alloc_ { alloc }
        , operation_ { std::forward<Args>(args)... }
    {
    }

    [[nodiscard]] auto perform_io() noexcept -> bool override
    {
        return operation_.io(get_fd());
    }

    auto do_cancel() noexcept -> void override { operation_.cancel(); }

    auto discard() noexcept -> void override
    {
        using Self = AsyncIoOperationImpl;
        using SelfAlloc =
            std::allocator_traits<Alloc>::template rebind_alloc<Self>;
        SelfAlloc alloc_tmp { alloc_ };
        this->~Self();
        alloc_tmp.deallocate(this, 1);
    }

    auto dispatch() -> void override
    {
        auto f { std::move(f_) };
        auto operation { std::move(operation_) };
        this->discard();
        std::move(operation).dispatch(std::move(f));
    }

private:
    F f_;
    Alloc alloc_;
    Operation operation_;
};

template <typename OperationTag, typename F, typename Alloc, typename... Args>
[[nodiscard]] auto make_async_io_operation(OperationTag const&,
                                           F&& f,
                                           Alloc const& alloc,
                                           Context ctx,
                                           int fd,
                                           Args&&... args)
    -> std::add_pointer_t<
        AsyncIoOperationImpl<OperationTag, std::decay_t<F>, Alloc>>
{
    using Self = AsyncIoOperationImpl<OperationTag, std::decay_t<F>, Alloc>;
    using SelfAlloc = std::allocator_traits<Alloc>::template rebind_alloc<Self>;
    SelfAlloc alloc_tmp { alloc };

    auto* ptr = alloc_tmp.allocate(1);

    try {
        new (static_cast<void*>(ptr)) Self {
            std::forward<F>(f), alloc, ctx, fd, std::forward<Args>(args)...
        };
    }
    catch (...) {
        alloc_tmp.deallocate(ptr, 1);
        throw;
    }

    return ptr;
}

} // namespace exios

#endif // EXIOS_ASYNC_IO_OPERATION_HPP_INCLUDED
