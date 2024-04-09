#ifndef EXIOS_ASYNC_OPERATION_HPP_INCLUDED
#define EXIOS_ASYNC_OPERATION_HPP_INCLUDED

#include "exios/intrusive_list.hpp"
#include "exios/result.hpp"
#include <memory>
#include <optional>
#include <system_error>
#include <type_traits>
#include <utility>

namespace exios
{

struct AnyAsyncOperation : ListItemBase
{
    virtual ~AnyAsyncOperation() {}
    virtual auto dispatch() -> void = 0;
    virtual auto discard() noexcept -> void = 0;
};

template <typename F, typename Alloc>
struct AsyncOperationImpl final : AnyAsyncOperation
{
    AsyncOperationImpl(F&& f, Alloc const& alloc);

    auto dispatch() -> void override;
    auto discard() noexcept -> void override;

protected:
    F f_;
    Alloc alloc_;
};

template <typename F, typename Alloc>
AsyncOperationImpl<F, Alloc>::AsyncOperationImpl(F&& f, Alloc const& alloc)
    : f_ { std::forward<F>(f) }
    , alloc_ { alloc }
{
}

template <typename F, typename Alloc>
auto AsyncOperationImpl<F, Alloc>::dispatch() -> void
{
    auto tmp_f_ { std::move(f_) };
    discard();
    tmp_f_();
}

template <typename F, typename Alloc>
auto AsyncOperationImpl<F, Alloc>::discard() noexcept -> void
{
    using Self = AsyncOperationImpl<F, Alloc>;
    using SelfAlloc = std::allocator_traits<Alloc>::template rebind_alloc<Self>;
    SelfAlloc alloc_tmp { alloc_ };
    this->~Self();
    alloc_tmp.deallocate(this, 1);
}

template <typename F, typename Alloc>
[[nodiscard]] auto make_async_operation(F&& f, Alloc const& alloc)
    -> std::add_pointer_t<AsyncOperationImpl<std::decay_t<F>, Alloc>>
{
    using Self = AsyncOperationImpl<F, Alloc>;
    using SelfAlloc = std::allocator_traits<Alloc>::template rebind_alloc<Self>;
    SelfAlloc alloc_tmp { alloc };

    auto* ptr = alloc_tmp.allocate(1);

    try {
        new (static_cast<void*>(ptr)) Self { std::forward<F>(f), alloc };
    }
    catch (...) {
        alloc_tmp.deallocate(ptr, 1);
        throw;
    }

    return ptr;
}

auto discard(AnyAsyncOperation&& op) noexcept -> void;
auto dispatch(AnyAsyncOperation&& op) -> void;

} // namespace exios

#endif // EXIOS_ASYNC_OPERATION_HPP_INCLUDED
