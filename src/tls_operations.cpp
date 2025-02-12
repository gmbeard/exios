#include "exios/tls_operations.hpp"
#include "openssl/ssl.h"
#include <memory>

namespace exios::detail::tls_transfer_
{
auto TransferRead::operator()(SSLState const& state,
                              BufferView buffer,
                              std::size_t& transferred) const noexcept -> int
{
    return SSL_read_ex(
        state.ssl(), buffer.data, buffer.size, std::addressof(transferred));
}

auto TransferWrite::operator()(SSLState const& state,
                               ConstBufferView buffer,
                               std::size_t& transferred) const noexcept -> int
{
    return SSL_write_ex(
        state.ssl(), buffer.data, buffer.size, std::addressof(transferred));
}
} // namespace exios::detail::tls_transfer_
