#ifndef EXIOS_ALLOC_UTILS_HPP_INCLUDED
#define EXIOS_ALLOC_UTILS_HPP_INCLUDED

#include <memory>
#include <type_traits>
#include <utility>

namespace exios
{

// clang-format off
template<typename T>
concept HasMemberAllocator = requires (T val) {
    { val.get_allocator() };
};
// clang-format on

template <typename T, typename Alloc>
auto rebind_allocator(Alloc const& alloc)
{
    using ReboundAlloc =
        std::allocator_traits<Alloc>::template rebind_alloc<std::decay_t<T>>;

    return ReboundAlloc { alloc };
}

template <HasMemberAllocator F, typename Alloc = std::allocator<void>>
auto select_allocator(F&& f, Alloc const& = Alloc {})
{
    return rebind_allocator<F>(f.get_allocator());
}

template <typename F, typename Alloc = std::allocator<void>>
auto select_allocator(F&&, Alloc const& fallback = Alloc {})
requires(!HasMemberAllocator<F>)
{
    return rebind_allocator<F>(fallback);
}

namespace detail
{
template <typename F, typename Alloc>
struct UseAllocatorWrapper
{
    UseAllocatorWrapper(F&& f, Alloc const& alloc) noexcept
        : f_ { std::move(f) }
        , alloc_ { alloc }
    {
    }

    UseAllocatorWrapper(F const& f, Alloc const& alloc) noexcept
    requires(std::is_copy_constructible_v<F>)
        : f_ { f }
        , alloc_ { alloc }
    {
    }

    template <typename... Args>
    auto operator()(Args&&... args) const
    {
        return f_(std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto operator()(Args&&... args)
    {
        return f_(std::forward<Args>(args)...);
    }

    auto get_allocator() const noexcept -> Alloc const& { return alloc_; }

private:
    F f_;
    Alloc alloc_;
};

} // namespace detail

template <typename F, typename Alloc>
auto use_allocator(F&& f, Alloc const& alloc)
{
    return detail::UseAllocatorWrapper { std::forward<F>(f), alloc };
}

} // namespace exios

#endif // EXIOS_ALLOC_UTILS_HPP_INCLUDED
