#include "exios/tcp_socket.hpp"
#include "exios/contracts.hpp"
#include "exios/io_scheduler.hpp"
#include "exios/utils.hpp"
#include <cstdint>
#include <errno.h>
#include <sys/socket.h>
#include <system_error>

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

} // namespace exios
