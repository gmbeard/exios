#ifndef EXIOS_TESTS_TRACKING_ALLOCATOR_HPP_INCLUDED
#define EXIOS_TESTS_TRACKING_ALLOCATOR_HPP_INCLUDED

#include <memory>
#include <type_traits>

template <typename T, typename F, typename Inner = std::allocator<T>>
struct TrackingAllocator
{
    using value_type = typename Inner::value_type;

    TrackingAllocator(F& f, Inner const& alloc = Inner {})
        : f_ { &f }
        , alloc_ { alloc }
    {
    }

    template <typename U, typename InnerU>
    TrackingAllocator(TrackingAllocator<U, F, InnerU> const& other) noexcept
        : f_ { other.f_ }
        , alloc_ { other.alloc_ }
    {
    }

    template <typename U>
    struct rebind
    {
        using other = TrackingAllocator<
            U,
            F,
            typename std::allocator_traits<Inner>::template rebind_alloc<U>>;
    };

    auto allocate(std::size_t n)
    {
        (*f_)(true, n);
        return alloc_.allocate(n);
    }

    auto deallocate(T* ptr, std::size_t n)
    {
        (*f_)(false, n);
        return alloc_.deallocate(ptr, n);
    }

    F* f_;
    Inner alloc_;
};

template <typename T, typename F>
auto tracking_allocator(F&& f)
{
    return TrackingAllocator<T, std::decay_t<F>, std::allocator<T>> {
        std::forward<F>(f)
    };
}

#endif // EXIOS_TESTS_TRACKING_ALLOCATOR_HPP_INCLUDED
