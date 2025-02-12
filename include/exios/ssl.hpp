#ifndef EXIOS_SSL_HPP_INCLUDED
#define EXIOS_SSL_HPP_INCLUDED

#include "openssl/ssl.h"
#include <cstddef>

namespace exios
{

inline constexpr std::size_t kMaxTLSRecordSize =
    static_cast<std::size_t>(1 << 14);

auto initialize_ssl() -> void;

[[noreturn]] auto throw_ssl_error() -> void;
[[noreturn]] auto throw_ssl_error(SSL*, int) -> void;

[[nodiscard]] auto ssl_context() noexcept -> SSL_CTX*;

} // namespace exios

#endif // EXIOS_SSL_HPP_INCLUDED
