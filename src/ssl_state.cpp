#include "exios/ssl_state.hpp"
#include "exios/contracts.hpp"
#include "exios/ssl.hpp"
#include <utility>

namespace exios
{

namespace detail
{
SSLState::SSLState(SSLMode mode)
    : transport_receive_buffer_(kMaxTLSRecordSize)
    , transport_send_buffer_(kMaxTLSRecordSize)
{
    initialize_ssl();
    if (ssl_ = SSL_new(ssl_context()); ssl_ == nullptr) {
        throw_ssl_error();
    }

    if (mode == SSLMode::client) {
        SSL_set_connect_state(ssl_);
    }
    else {
        SSL_set_accept_state(ssl_);
    }

    if (int const r = BIO_new_bio_pair(&internal_bio_, 0, &transport_bio_, 0);
        r != 1) {
        throw_ssl_error();
    }

    SSL_set_bio(ssl_, internal_bio_, internal_bio_);
}

SSLState::SSLState(SSLState&& other) noexcept
    : ssl_ { std::exchange(other.ssl_, nullptr) }
    , internal_bio_ { std::exchange(other.internal_bio_, nullptr) }
    , transport_bio_ { std::exchange(other.transport_bio_, nullptr) }
    , transport_receive_buffer_ { std::move(other.transport_receive_buffer_) }
    , transport_send_buffer_ { std::move(other.transport_send_buffer_) }
    , transport_receive_size_ { other.transport_receive_size_ }
    , transport_send_size_ { other.transport_send_size_ }
{
}

SSLState::~SSLState()
{
    if (ssl_) {
        SSL_free(ssl_);
    }

    if (transport_bio_) {
        BIO_free(transport_bio_);
    }
}

auto SSLState::operator=(SSLState&& rhs) noexcept -> SSLState&
{
    SSLState tmp { std::move(rhs) };
    swap(*this, tmp);
    return *this;
}

auto swap(SSLState& lhs, SSLState& rhs) noexcept -> void
{
    using std::swap;
    swap(lhs.ssl_, rhs.ssl_);
    swap(lhs.internal_bio_, rhs.internal_bio_);
    swap(lhs.transport_bio_, rhs.transport_bio_);
    swap(lhs.transport_receive_buffer_, rhs.transport_receive_buffer_);
    swap(lhs.transport_send_buffer_, rhs.transport_send_buffer_);
    swap(lhs.transport_receive_size_, rhs.transport_receive_size_);
    swap(lhs.transport_send_size_, rhs.transport_send_size_);
}

auto SSLState::mode() const noexcept -> SSLMode
{
    EXIOS_EXPECT(ssl_);
    return SSL_is_server(ssl_) ? SSLMode::server : SSLMode::client;
}

auto SSLState::ssl() const noexcept -> SSL*
{
    EXIOS_EXPECT(ssl_);
    return ssl_;
}

auto SSLState::transport_bio() const noexcept -> BIO*
{
    EXIOS_EXPECT(transport_bio_);
    return transport_bio_;
}

auto SSLState::transport_receive_buffer() noexcept -> std::span<std::byte>
{
    EXIOS_EXPECT(transport_receive_size_ <= transport_receive_buffer_.size());
    std::span buf { transport_receive_buffer_.data(),
                    transport_receive_buffer_.size() };
    return buf.subspan(transport_receive_size_);
}

auto SSLState::transport_send_buffer() noexcept -> std::span<std::byte>
{
    EXIOS_EXPECT(transport_send_size_ <= transport_send_buffer_.size());
    return { transport_send_buffer_.data(), transport_send_size_ };
}

auto SSLState::commit_receive_buffer_bytes(std::size_t n) -> void
{
    transport_receive_size_ += n;
    EXIOS_EXPECT(transport_receive_size_ <= transport_receive_buffer_.size());
    int const r = BIO_write(transport_bio(),
                            transport_receive_buffer_.data(),
                            static_cast<int>(transport_receive_size_));
    /* NOTE:
     * Should BIO_write always succeed??
     */
    EXIOS_EXPECT(r >= 0);

    std::copy(
        std::next(transport_receive_buffer_.begin(),
                  static_cast<std::size_t>(r)),
        std::next(transport_receive_buffer_.begin(), transport_receive_size_),
        transport_receive_buffer_.begin());

    transport_receive_size_ -= static_cast<std::size_t>(r);
}

auto SSLState::prepare_send_buffer() -> void
{
    auto buf = std::span { transport_send_buffer_.data(),
                           transport_send_buffer_.size() };
    buf = buf.subspan(transport_send_size_);

    auto const n = BIO_read(transport_bio(), buf.data(), buf.size());
    if (n <= 0) {
        return;
    }

    transport_send_size_ += static_cast<std::size_t>(n);
    EXIOS_EXPECT(transport_send_size_ <= transport_send_buffer_.size());
}

auto SSLState::consume_send_buffer(std::size_t n) noexcept -> void
{
    EXIOS_EXPECT(n <= transport_send_size_);
    std::copy(std::next(transport_send_buffer_.begin(), n),
              std::next(transport_send_buffer_.begin(), transport_send_size_),
              transport_send_buffer_.begin());

    transport_send_size_ -= n;
}

} // namespace detail

} // namespace exios
