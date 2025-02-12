#ifndef EXIOS_TLS_OPERATIONS_HPP_INCLUDED
#define EXIOS_TLS_OPERATIONS_HPP_INCLUDED

#include "exios/alloc_utils.hpp"
#include "exios/buffer_view.hpp"
#include "exios/io.hpp"
#include "exios/ssl_state.hpp"
#include <openssl/ssl.h>
#include <system_error>
#include <utility>

namespace exios
{

namespace detail
{

namespace write_to_peer_
{

template <typename AsyncWriteStream, typename Completion>
struct WriteToPeerOperation
{
    AsyncWriteStream& stream;
    SSLState& state;
    Completion completion;

    auto on_complete(IoResult result) -> void
    {
        if (result) {
            state.consume_send_buffer(result.value());
        }

        std::move(completion)(std::move(result));
    }

    auto operator()() -> void
    {
        auto const alloc = select_allocator(completion);
        state.prepare_send_buffer();
        stream.write(const_buffer_view(state.transport_send_buffer()),
                     use_allocator(
                         [self = std::move(*this)](auto r) mutable {
                             self.on_complete(std::move(r));
                         },
                         alloc));
    }
};

struct WriteToPeer
{
    template <typename AsyncWriteStream, typename Completion>
    auto operator()(AsyncWriteStream& stream,
                    SSLState& state,
                    Completion&& completion) const noexcept -> void
    {
        WriteToPeerOperation op { stream,
                                  state,
                                  std::forward<Completion>(completion) };
        op();
    }
};

} // namespace write_to_peer_

namespace read_from_peer_
{
template <typename AsyncReadStream, typename Completion>
struct ReadFromPeerOperation
{
    AsyncReadStream& stream;
    SSLState& state;
    Completion completion;

    auto on_complete(IoResult result) -> void
    {
        if (result) {
            state.commit_receive_buffer_bytes(result.value());
        }

        std::move(completion)(std::move(result));
    }

    auto operator()() -> void
    {
        auto const alloc = select_allocator(completion);
        stream.read(buffer_view(state.transport_receive_buffer()),
                    use_allocator(
                        [self = std::move(*this)](auto r) mutable {
                            self.on_complete(std::move(r));
                        },
                        alloc));
    }
};

struct ReadFromPeer
{
    template <typename AsyncWriteStream, typename Completion>
    auto operator()(AsyncWriteStream& stream,
                    SSLState& state,
                    Completion&& completion) const noexcept -> void
    {
        ReadFromPeerOperation op { stream,
                                   state,
                                   std::forward<Completion>(completion) };
        op();
    }
};

} // namespace read_from_peer_

inline constexpr write_to_peer_::WriteToPeer write_to_peer {};
inline constexpr read_from_peer_::ReadFromPeer read_from_peer {};

namespace tls_handshake_
{

template <typename AsyncReadWriteStream, typename Completion>
struct TlsHandshakeOperation
{
    AsyncReadWriteStream& stream;
    SSLState& state;
    Completion completion;
    bool in_same_stack_frame { true };
    using Allocator = std::decay_t<decltype(select_allocator(completion))>;
    Allocator allocator { select_allocator(completion) };
    using ResultType = Result<void, std::error_code>;

    // clang-format off
    static constexpr struct PeerRead { } peer_read {};
    static constexpr struct PeerWrite { } peer_write {};
    // clang-format on

    auto set_complete(ResultType result) -> void
    {
        if (!in_same_stack_frame) {
            std::move(completion)(std::move(result));
            return;
        }

        auto context = stream.get_context();
        context.post(
            [c = std::move(completion), r = std::move(result)]() mutable {
                std::move(c)(std::move(r));
            },
            allocator);
    }

    auto set_complete() -> void
    {
        set_complete(ResultType {});
    }

    auto set_complete(std::error_code ec) -> void
    {
        set_complete(ResultType { result_error(ec) });
    }

    auto peer_transfer(PeerRead) -> void
    {
        read_from_peer(stream,
                       state,
                       use_allocator(
                           [self = std::move(*this)](auto result) mutable {
                               self.in_same_stack_frame = false;
                               if (!result) {
                                   self.set_complete(result.error());
                                   return;
                               }
                               self();
                           },
                           allocator));
    }

    auto peer_transfer(PeerWrite) -> void
    {
        state.prepare_send_buffer();
        if (state.transport_send_buffer().size() == 0) {
            peer_transfer(peer_read);
            return;
        }

        write_to_peer(stream,
                      state,
                      use_allocator(
                          [self = std::move(*this)](auto result) mutable {
                              self.in_same_stack_frame = false;
                              if (!result) {
                                  self.set_complete(result.error());
                                  return;
                              }
                              self.peer_transfer(peer_read);
                          },
                          allocator));
    }

    auto operator()() -> void
    {
        int const n = SSL_do_handshake(state.ssl());
        if (n > 0) {
            EXIOS_EXPECT(SSL_is_init_finished(state.ssl()));
            set_complete();
            return;
        }

        auto const status = SSL_get_error(state.ssl(), n);
        switch (status) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            peer_transfer(peer_write);
            break;
        default:
            /* HACK:
             * We're just transposing SSL_ERROR_* values directly
             * into system error codes. Is this ok??
             */
            set_complete(std::error_code { status, std::system_category() });
            break;
        }
    }
};

struct TlsHandshake
{
    template <typename AsyncReadWriteStream, typename Completion>
    auto operator()(AsyncReadWriteStream& stream,
                    SSLState& state,
                    Completion&& completion) const -> void
    {
        TlsHandshakeOperation op { stream,
                                   state,
                                   std::forward<Completion>(completion) };
        op();
    }
};

} // namespace tls_handshake_

namespace tls_transfer_
{

struct TransferRead
{
    auto operator()(SSLState const& state,
                    BufferView buffer,
                    std::size_t& transferred) const noexcept -> int;
};

struct TransferWrite
{
    auto operator()(SSLState const& state,
                    ConstBufferView buffer,
                    std::size_t& transferred) const noexcept -> int;
};

template <typename AsyncReadWriteStream, typename Transfer, typename Completion>
struct TlsTransferOperation
{
    AsyncReadWriteStream& stream;
    SSLState& state;
    Transfer transfer;
    using BufferType =
        std::conditional_t<std::is_same_v<Transfer, TransferRead>,
                           BufferView,
                           ConstBufferView>;
    BufferType buffer;
    Completion completion;
    bool in_same_stack_frame { true };
    using Allocator = std::decay_t<decltype(select_allocator(completion))>;
    Allocator allocator { select_allocator(completion) };
    using ResultType = IoResult;
    std::size_t bytes_transferred { 0 };

    // clang-format off
    static constexpr struct PeerRead { } peer_read {};
    static constexpr struct PeerWrite { } peer_write {};
    // clang-format on

    auto set_complete(ResultType result) -> void
    {
        if (!in_same_stack_frame) {
            std::move(completion)(std::move(result));
            return;
        }

        auto context = stream.get_context();
        context.post(
            [c = std::move(completion), r = std::move(result)]() mutable {
                std::move(c)(std::move(r));
            },
            allocator);
    }

    auto set_complete() -> void
    {
        set_complete(ResultType { result_ok(bytes_transferred) });
    }

    auto set_complete(std::error_code ec) -> void
    {
        set_complete(ResultType { result_error(ec) });
    }

    auto peer_transfer(PeerRead) -> void
    {
        read_from_peer(stream,
                       state,
                       use_allocator(
                           [self = std::move(*this)](auto result) mutable {
                               self.in_same_stack_frame = false;
                               if (!result) {
                                   self.set_complete(result.error());
                                   return;
                               }
                               self();
                           },
                           allocator));
    }

    auto peer_transfer(PeerWrite) -> void
    {
        write_to_peer(stream,
                      state,
                      use_allocator(
                          [self = std::move(*this)](auto result) mutable {
                              self.in_same_stack_frame = false;
                              if (!result) {
                                  self.set_complete(result.error());
                                  return;
                              }
                              self();
                          },
                          allocator));
    }

    auto operator()() -> void
    {
        state.prepare_send_buffer();
        if (state.transport_send_buffer().size() > 0) {
            peer_transfer(peer_write);
            return;
        }

        bytes_transferred = 0;
        int const n = transfer(state, buffer, bytes_transferred);

        if (n > 0) {
            set_complete();
            return;
        }

        auto const status = SSL_get_error(state.ssl(), n);
        switch (status) {
        case SSL_ERROR_WANT_READ:
            peer_transfer(peer_read);
            break;
        case SSL_ERROR_WANT_WRITE:
            peer_transfer(peer_write);
            break;
        case SSL_ERROR_ZERO_RETURN:
            set_complete();
            break;
        default:
            /* HACK:
             * We're just transposing SSL_ERROR_* values directly
             * into system error codes. Is this ok??
             */
            set_complete(std::error_code { status, std::system_category() });
            break;
        }
    }
};

template <typename Transfer>
requires(std::is_same_v<Transfer, TransferRead> ||
         std::is_same_v<Transfer, TransferWrite>)
struct TlsTransfer
{
    using BufferType =
        std::conditional_t<std::is_same_v<Transfer, TransferRead>,
                           BufferView,
                           ConstBufferView>;

    template <typename AsyncReadWriteStream, typename Completion>
    auto operator()(AsyncReadWriteStream& stream,
                    SSLState& state,
                    BufferType buffer,
                    Completion&& completion) const -> void
    {
        TlsTransferOperation op { stream,
                                  state,
                                  Transfer {},
                                  buffer,
                                  std::forward<Completion>(completion) };
        op();
    }
};
} // namespace tls_transfer_

inline constexpr tls_transfer_::TlsTransfer<tls_transfer_::TransferWrite>
    tls_write {};
inline constexpr tls_transfer_::TlsTransfer<tls_transfer_::TransferRead>
    tls_read {};

namespace tls_shutdown_
{
template <typename AsyncReadWriteStream, typename Completion>
struct TlsShutdownOperation
{
    AsyncReadWriteStream& stream;
    SSLState& state;
    Completion completion;
    bool in_same_stack_frame { true };
    using Allocator = std::decay_t<decltype(select_allocator(completion))>;
    Allocator allocator { select_allocator(completion) };
    using ResultType = Result<void, std::error_code>;
    std::size_t bytes_transferred { 0 };

    // clang-format off
    static constexpr struct PeerRead { } peer_read {};
    static constexpr struct PeerWrite { } peer_write {};
    // clang-format on

    auto set_complete(ResultType result) -> void
    {
        if (!in_same_stack_frame) {
            std::move(completion)(std::move(result));
            return;
        }

        auto context = stream.get_context();
        context.post(
            [c = std::move(completion), r = std::move(result)]() mutable {
                std::move(c)(std::move(r));
            },
            allocator);
    }

    auto set_complete() -> void
    {
        set_complete(ResultType {});
    }

    auto set_complete(std::error_code ec) -> void
    {
        set_complete(ResultType { result_error(ec) });
    }

    auto peer_transfer(PeerRead) -> void
    {
        read_from_peer(stream,
                       state,
                       use_allocator(
                           [self = std::move(*this)](auto result) mutable {
                               self.in_same_stack_frame = false;
                               if (!result) {
                                   self.set_complete(result.error());
                                   return;
                               }
                               self();
                           },
                           allocator));
    }

    auto peer_transfer(PeerWrite) -> void
    {
        write_to_peer(stream,
                      state,
                      use_allocator(
                          [self = std::move(*this)](auto result) mutable {
                              self.in_same_stack_frame = false;
                              if (!result) {
                                  self.set_complete(result.error());
                                  return;
                              }
                              self();
                          },
                          allocator));
    }

    auto do_read() -> void
    {
        BufferView scratch { nullptr, 0 };
        auto const alloc = allocator;
        tls_read(stream,
                 state,
                 scratch,
                 use_allocator(
                     [self = std::move(*this)](exios::IoResult result) mutable {
                         if (!result) {
                             self.set_complete(result.error());
                         }
                         else {
                             self.set_complete();
                         }
                     },
                     alloc));
    }

    auto operator()() -> void
    {
        state.prepare_send_buffer();
        if (state.transport_send_buffer().size() > 0) {
            peer_transfer(peer_write);
            return;
        }

        bytes_transferred = 0;
        int const n = SSL_shutdown(state.ssl());

        if (n > 0) {
            set_complete();
            return;
        }

        auto const status = SSL_get_error(state.ssl(), n);
        switch (status) {
        case SSL_ERROR_WANT_READ:
            peer_transfer(peer_read);
            break;
        case SSL_ERROR_WANT_WRITE:
            peer_transfer(peer_write);
            break;
        default:
            if (n < 0) {
                set_complete(
                    std::error_code { status, std::system_category() });
            }
            else {
                do_read();
            }
            break;
        }
    }
};

struct TlsShutdown
{
    template <typename AsyncReadWriteStream, typename Completion>
    auto operator()(AsyncReadWriteStream& stream,
                    SSLState& state,
                    Completion&& completion) const -> void
    {
        TlsShutdownOperation op { stream,
                                  state,
                                  std::forward<Completion>(completion) };
        op();
    }
};

} // namespace tls_shutdown_

inline constexpr tls_handshake_::TlsHandshake tls_handshake {};
inline constexpr tls_shutdown_::TlsShutdown tls_shutdown {};

} // namespace detail

} // namespace exios

#endif // EXIOS_TLS_OPERATIONS_HPP_INCLUDED
