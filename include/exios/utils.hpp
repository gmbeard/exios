#ifndef EXIOS_UTILS_HPP_INCLUDED
#define EXIOS_UTILS_HPP_INCLUDED

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string_view>
#include <type_traits>

namespace exios
{
template <typename T>
auto reverse_byte_order(T const& val) noexcept -> T
requires(std::is_integral_v<T>)
{
    std::array<unsigned char, sizeof(T)> bytes;
    std::copy_n(reinterpret_cast<unsigned char const*>(&val),
                sizeof(T),
                std::rbegin(bytes));

    T result;
    std::memcpy(std::addressof(result), bytes.data(), bytes.size());
    return result;
}

template <typename T>
auto parse_integer(char const* first, char const* last, T& outval) noexcept
    -> bool
requires(std::is_integral_v<T>)
{
    if (first == last)
        return false;

    outval = T {};

    for (; first != last; ++first) {
        if (*first < '0' || *first > '9')
            return false;

        outval = outval * 10 + static_cast<T>(*first - '0');
    }

    return true;
}

auto parse_ipv4(std::string_view address) -> std::uint32_t;

} // namespace exios

#endif // EXIOS_UTILS_HPP_INCLUDED
