#include "exios/tcp_socket.hpp"
#include "exios/contracts.hpp"
#include "exios/io_scheduler.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <errno.h>
#include <iterator>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>
#include <type_traits>

namespace
{
template <typename T>
auto reverse_byte_order(T const& val) noexcept -> T
requires(std::is_integral_v<T>)
{
    std::array<unsigned char, sizeof(T)> bytes;
    std::copy_n(reinterpret_cast<unsigned char const*>(&val),
                sizeof(T),
                std::rbegin(bytes));

    return std::bit_cast<T>(bytes);
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

} // namespace

namespace exios
{

TcpSocket::TcpSocket(Context const& ctx)
    : IoObject {
        ctx, ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)
    }
{
    if (fd_.value() < 0)
        throw std::system_error { errno, std::system_category() };
}

TcpSocket::TcpSocket(Context const& ctx, int fd) noexcept
    : IoObject { ctx, fd }
{
}

auto TcpSocket::cancel() -> void { ctx_.io_scheduler().cancel(fd_.value()); }

TcpSocketAcceptor::TcpSocketAcceptor(Context const& ctx,
                                     std::uint16_t port,
                                     std::string_view address)
    : IoObject { ctx,
                 ::socket(
                     AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0) }
    , addr_ {}
{
    if (fd_.value() < 0)
        throw std::system_error { errno, std::system_category() };

    int const optval = 1;
    if (::setsockopt(
            fd_.value(), SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)))
        throw std::system_error { errno, std::system_category() };

    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = reverse_byte_order(parse_ipv4(address));
    addr_.sin_port = reverse_byte_order(port);

    if (auto const result = ::bind(fd_.value(),
                                   reinterpret_cast<sockaddr const*>(&addr_),
                                   sizeof(addr_));
        result < 0)
        throw std::system_error { errno, std::system_category() };

    if (auto const result = ::listen(fd_.value(), SOMAXCONN); result < 0)
        throw std::system_error { errno, std::system_category() };
}

TcpSocketAcceptor::TcpSocketAcceptor(Context const& ctx, std::uint16_t port)
    : TcpSocketAcceptor { ctx, port, "0.0.0.0" }
{
}

auto TcpSocketAcceptor::address() const noexcept -> std::string_view
{
    EXIOS_EXPECT(false && "Not implemented!");
}

auto TcpSocketAcceptor::port() const noexcept -> std::uint16_t
{
    return reverse_byte_order(addr_.sin_port);
}

auto TcpSocketAcceptor::cancel() noexcept -> void
{
    ctx_.io_scheduler().cancel(fd_.value());
}

} // namespace exios
