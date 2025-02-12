#ifndef EXIOS_SSL_STATE_HPP_INCLUDED
#define EXIOS_SSL_STATE_HPP_INCLUDED

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <span>
#include <vector>

namespace exios
{
enum class SSLMode
{
    client,
    server
};

namespace detail
{

struct SSLState
{
    explicit SSLState(SSLMode mode);
    SSLState(SSLState&& other) noexcept;
    ~SSLState();
    auto operator=(SSLState&& rhs) noexcept -> SSLState&;
    auto mode() const noexcept -> SSLMode;

    auto ssl() const noexcept -> SSL*;
    auto transport_bio() const noexcept -> BIO*;

    auto transport_receive_buffer() noexcept -> std::span<std::byte>;
    auto transport_send_buffer() noexcept -> std::span<std::byte>;

    auto commit_receive_buffer_bytes(std::size_t n) -> void;
    auto prepare_send_buffer() -> void;
    auto consume_send_buffer(std::size_t n) noexcept -> void;

    SSL* ssl_ { nullptr };
    BIO* internal_bio_ { nullptr };
    BIO* transport_bio_ { nullptr };
    std::vector<std::byte> transport_receive_buffer_;
    std::vector<std::byte> transport_send_buffer_;
    std::size_t transport_receive_size_ { 0 };
    std::size_t transport_send_size_ { 0 };
};

auto swap(SSLState& lhs, SSLState& rhs) noexcept -> void;

} // namespace detail
} // namespace exios

#endif // EXIOS_SSL_STATE_HPP_INCLUDED
