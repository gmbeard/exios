#ifndef EXIOS_TLS_TCP_SOCKET_HPP_INCLUDED
#define EXIOS_TLS_TCP_SOCKET_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/buffer_view.hpp"
#include "exios/io.hpp"
#include "exios/ssl_state.hpp"
#include "exios/tcp_socket.hpp"
#include "exios/tls_operations.hpp"
#include <algorithm>
#include <cstdint>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <string_view>

namespace exios
{

struct TlsTcpSocket
{
    explicit TlsTcpSocket(TcpSocket&& transport, SSLMode mode);
    TlsTcpSocket(exios::Context const& ctx, SSLMode mode);

    template <typename Completion>
    auto connect(std::string_view address,
                 std::uint16_t port,
                 Completion&& completion) -> void
    {
        auto const alloc = exios::select_allocator(completion);
        auto f = [this, completion = std::move(completion)](
                     exios::ConnectResult result) mutable {
            if (!result) {
                std::move(completion)(std::move(result));
                return;
            }

            detail::tls_handshake(transport_socket_,
                                  ssl_state_,
                                  std::forward<Completion>(completion));
        };

        transport_socket_.connect(
            address, port, exios::use_allocator(std::move(f), alloc));
    }

    template <typename Completion>
    auto read(BufferView buffer, Completion&& completion) -> void
    {
        detail::tls_read(transport_socket_,
                         ssl_state_,
                         buffer,
                         std::forward<Completion>(completion));
    }

    template <typename Completion>
    auto write(ConstBufferView buffer, Completion&& completion) -> void
    {
        detail::tls_write(transport_socket_,
                          ssl_state_,
                          buffer,
                          std::forward<Completion>(completion));
    }

    template <typename Completion>
    auto shutdown(Completion&& completion) -> void
    {
        detail::tls_shutdown(transport_socket_,
                             ssl_state_,
                             std::forward<Completion>(completion));
    }

    auto native_ssl_handle() const noexcept -> SSL*;

    auto cancel() noexcept -> void;

private:
    TcpSocket transport_socket_;
    /* TODO:
     * It might be safer to put ssl_state_ on the heap. We're
     * heap allocating the send/receive buffers anyway...
     */
    detail::SSLState ssl_state_;
};

} // namespace exios

#endif // EXIOS_TLS_TCP_SOCKET_HPP_INCLUDED
