#ifndef EXIOS_BUFFER_VIEW_HPP_INCLUDED
#define EXIOS_BUFFER_VIEW_HPP_INCLUDED

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace exios
{
struct BufferView
{
    void* data { nullptr };
    std::size_t size { 0 };
};

struct ConstBufferView
{
    void const* data { nullptr };
    std::size_t size { 0 };
};

template <typename T>
requires(!std::is_const_v<T> && std::is_trivially_copyable_v<T>)
auto buffer_view(std::span<T> val) noexcept
{
    return BufferView { val.data(), sizeof(T) * val.size() };
}

template <typename T>
requires(std::is_trivially_copyable_v<T>)
auto const_buffer_view(std::span<T> val) noexcept
{
    return ConstBufferView { val.data(), sizeof(T) * val.size() };
}

template <typename T>
requires(!std::is_const_v<T> && std::is_trivially_copyable_v<T>)
auto buffer_view(std::vector<T>& val) noexcept
{
    return BufferView { val.data(), sizeof(T) * val.size() };
}

template <typename T>
requires(std::is_trivially_copyable_v<T>)
auto const_buffer_view(std::vector<T> const& val) noexcept
{
    return ConstBufferView { val.data(), sizeof(T) * val.size() };
}

template <typename T>
auto const_buffer_view(std::vector<T> const&& val) noexcept
    -> ConstBufferView = delete;

template <typename T, typename Traits, typename Allocator>
requires(std::is_trivially_copyable_v<T>)
auto const_buffer_view(std::basic_string_view<T, Traits> val) noexcept
    -> ConstBufferView
{
    return ConstBufferView { val.data(), sizeof(T) * val.size() };
}

template <typename T, typename Traits, typename Allocator>
requires(!std::is_const_v<T> && std::is_trivially_copyable_v<T>)
auto buffer_view(std::basic_string<T, Traits, Allocator>& val) noexcept
    -> BufferView
{
    return BufferView { val.data(), sizeof(T) * val.size() };
}

template <typename T, typename Traits, typename Allocator>
requires(std::is_trivially_copyable_v<T>)
auto const_buffer_view(
    std::basic_string<T, Traits, Allocator> const& val) noexcept
    -> ConstBufferView
{
    return ConstBufferView { val.data(), sizeof(T) * val.size() };
}

template <typename T, typename Traits, typename Allocator>
auto const_buffer_view(std::basic_string<T, Traits, Allocator> const&&)
    -> ConstBufferView = delete;

template <typename T, std::size_t N>
requires(!std::is_const_v<T> && std::is_trivially_copyable_v<T>)
auto buffer_view(std::array<T, N>& val) noexcept
{
    return BufferView { val.data(), sizeof(T) * N };
}

template <typename T, std::size_t N>
requires(std::is_trivially_copyable_v<T>)
auto const_buffer_view(std::array<T, N> const& val) noexcept
{
    return ConstBufferView { val.data(), sizeof(T) * N };
}

template <typename T, std::size_t N>
auto const_buffer_view(std::array<T, N> const&& val) noexcept
    -> ConstBufferView = delete;

} // namespace exios

#endif // EXIOS_BUFFER_VIEW_HPP_INCLUDED
