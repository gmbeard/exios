#ifndef EXIOS_BUFFER_VIEW_HPP_INCLUDED
#define EXIOS_BUFFER_VIEW_HPP_INCLUDED

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
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
requires(!std::is_const_v<T>)
auto buffer_view(std::span<T> val) noexcept
{
    return BufferView { val.data(), val.size() };
}

template <typename T>
auto const_buffer_view(std::span<T> val) noexcept
{
    return ConstBufferView { val.data(), val.size() };
}

template <typename T>
requires(!std::is_const_v<T>)
auto buffer_view(std::vector<T>& val) noexcept
{
    return BufferView { val.data(), val.size() };
}

template <typename T>
auto const_buffer_view(std::vector<T>& val) noexcept
{
    return ConstBufferView { val.data(), val.size() };
}

auto const_buffer_view(std::string_view val) noexcept -> ConstBufferView;

template <typename T, std::size_t N>
requires(!std::is_const_v<T>)
auto buffer_view(std::array<T, N>& val) noexcept
{
    return BufferView { val.data(), N };
}

template <typename T, std::size_t N>
auto const_buffer_view(std::array<T, N>& val) noexcept
{
    return ConstBufferView { val.data(), N };
}

} // namespace exios

#endif // EXIOS_BUFFER_VIEW_HPP_INCLUDED
