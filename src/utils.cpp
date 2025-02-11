#include "exios/utils.hpp"
#include <stdexcept>

namespace exios
{
auto parse_ipv4(std::string_view address) -> std::uint32_t
{
    std::array<std::uint8_t, 4> octets;

    auto p = begin(address);
    auto last = end(address);
    auto saved = p;
    auto out_p = std::begin(octets);

    std::size_t num_octets = 0;
    p = std::find(p, last, '.');

    while (p != last && num_octets < 4) {
        if (!parse_integer<std::remove_reference_t<decltype(*out_p)>>(
                saved, p, *out_p++))
            throw new std::runtime_error {
                "Couldn't parse IPv4: Invalid octet"
            };

        num_octets += 1;
        saved = ++p;
        p = std::find(p, last, '.');
    }

    if (p != last)
        throw new std::runtime_error { "Couldn't parse IPv4: Too many octets" };

    if (!parse_integer<std::remove_reference_t<decltype(*out_p)>>(
            saved, p, *out_p++))
        throw new std::runtime_error { "Couldn't parse IPv4: Invalid octet" };

    num_octets += 1;
    if (num_octets != 4)
        throw new std::runtime_error { "Couldn't parse IPv4: Too few octets" };

    return static_cast<std::uint32_t>(octets[0] << 24) |
           static_cast<std::uint32_t>(octets[1] << 16) |
           static_cast<std::uint32_t>(octets[2] << 8) |
           static_cast<std::uint32_t>(octets[3]);
}
} // namespace exios
