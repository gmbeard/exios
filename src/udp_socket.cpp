#include "exios/udp_socket.hpp"
#include "exios/context.hpp"
#include "exios/utils.hpp"
#include <cstdint>
#include <netinet/in.h>
#include <sys/socket.h>

namespace exios
{

UdpSocket::UdpSocket(Context const& ctx)
    : IoObject {
        ctx, ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)
    }
{
    if (fd_.value() < 0)
        throw std::system_error { errno, std::system_category() };
}

auto UdpSocket::bind(std::uint16_t port, std::string_view address) -> void
{
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = reverse_byte_order(parse_ipv4(address));
    addr.sin_port = reverse_byte_order(port);

    if (auto const result = ::bind(fd_.value(),
                                   reinterpret_cast<sockaddr const*>(&addr),
                                   sizeof(addr));
        result < 0) {
        throw std::system_error { errno, std::system_category() };
    }
}

} // namespace exios
