#include "exios/tls_tcp_socket.hpp"
#include "exios/context.hpp"
#include "exios/tcp_socket.hpp"
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <utility>

namespace exios
{

TlsTcpSocket::TlsTcpSocket(TcpSocket&& transport, SSLMode mode)
    : transport_socket_ { std::move(transport) }
    , ssl_state_ { mode }
{
}

TlsTcpSocket::TlsTcpSocket(Context const& ctx, SSLMode mode)
    : TlsTcpSocket { TcpSocket { ctx }, mode }
{
}

auto TlsTcpSocket::native_ssl_handle() const noexcept -> SSL*
{
    return ssl_state_.ssl();
}

auto TlsTcpSocket::cancel() noexcept -> void
{
    transport_socket_.cancel();
}

} // namespace exios
