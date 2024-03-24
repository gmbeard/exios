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
struct WriteOperation
{
};
struct ReadOperation
{
};

constexpr WriteOperation write_operation {};
constexpr ReadOperation read_operation {};

struct AsyncIoOperation : AnyAsyncOperation
{
    using ResultType = std::size_t;
    using ErrorType = std::error_code;

    AsyncIoOperation(Context ctx, int fd, bool is_read_operation) noexcept;

    [[nodiscard]] virtual auto perform_io() const noexcept -> IoResult = 0;
    [[nodiscard]] auto get_context() noexcept -> Context&;
    [[nodiscard]] auto get_fd() const noexcept -> int;
    auto set_result(IoResult) noexcept -> void;
    [[nodiscard]] auto is_read_operation() const noexcept -> bool;

protected:
    std::optional<Result<ResultType, ErrorType>> result_;

private:
    Context ctx_;
    int fd_;
    bool is_read_;
};

template <typename T>
concept Direction =
    std::is_same_v<T, WriteOperation> || std::is_same_v<T, ReadOperation>;

template <Direction D>
using BufferType = std::conditional_t<std::is_same_v<D, WriteOperation>,
                                      ConstBufferView,
                                      BufferView>;

template <Direction D, typename F, typename Alloc>
struct AsyncIoOperationImpl : AsyncIoOperation
{
    AsyncIoOperationImpl(F&& f,
                         Alloc const& alloc,
                         Context ctx,
                         int fd,
                         BufferType<D> buffer) noexcept;

    [[nodiscard]] auto perform_io() const noexcept -> IoResult override;
    auto dispatch() -> void override;
    auto discard() noexcept -> void override;

private:
    F f_;
    Alloc alloc_;
    BufferType<D> buffer_;
};

template <Direction D, typename F, typename Alloc>
AsyncIoOperationImpl<D, F, Alloc>::AsyncIoOperationImpl(
    F&& f,
    Alloc const& alloc,
    Context ctx,
    int fd,
    BufferType<D> buffer) noexcept
    : AsyncIoOperation { ctx, fd, std::is_same_v<D, ReadOperation> }
    , f_ { std::move(f) }
    , alloc_ { alloc }
    , buffer_ { buffer }
{
}

template <Direction D, typename F, typename Alloc>
auto AsyncIoOperationImpl<D, F, Alloc>::perform_io() const noexcept -> IoResult
{
    if constexpr (std::is_same_v<D, WriteOperation>) {
        return perform_write(get_fd(), buffer_);
    }
    else {
        return perform_read(get_fd(), buffer_);
    }
}

template <Direction D, typename F, typename Alloc>
auto AsyncIoOperationImpl<D, F, Alloc>::dispatch() -> void
{
    auto tmp_f_ { std::move(f_) };
    auto tmp_result_ { std::move(result_).value() };

    discard();

    tmp_f_(std::move(tmp_result_));
}

template <Direction D, typename F, typename Alloc>
auto AsyncIoOperationImpl<D, F, Alloc>::discard() noexcept -> void
{
    using Self = AsyncIoOperationImpl<D, F, Alloc>;
    using SelfAlloc = std::allocator_traits<Alloc>::template rebind_alloc<Self>;
    SelfAlloc alloc_tmp { alloc_ };
    this->~Self();
    alloc_tmp.deallocate(this, 1);
}

template <Direction D, typename F, typename Alloc>
[[nodiscard]] auto make_async_io_operation(D const&,
                                           F&& f,
                                           Alloc const& alloc,
                                           Context ctx,
                                           int fd,
                                           BufferType<D> buffer)
    -> std::add_pointer_t<AsyncIoOperationImpl<D, std::decay_t<F>, Alloc>>
{
    using Self = AsyncIoOperationImpl<D, std::decay_t<F>, Alloc>;
    using SelfAlloc = std::allocator_traits<Alloc>::template rebind_alloc<Self>;
    SelfAlloc alloc_tmp { alloc };

    auto* ptr = alloc_tmp.allocate(1);

    try {
        new (static_cast<void*>(ptr))
            Self { std::forward<F>(f), alloc, ctx, fd, buffer };
    }
    catch (...) {
        alloc_tmp.deallocate(ptr, 1);
        throw;
    }

    return ptr;
}

} // namespace exios

#endif // EXIOS_ASYNC_IO_OPERATION_HPP_INCLUDED
