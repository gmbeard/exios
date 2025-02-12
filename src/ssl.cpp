#include "exios/ssl.hpp"
#include "exios/contracts.hpp"
#include "openssl/err.h"
#include "openssl/ssl.h"
#include <mutex>
#include <stdexcept>
#include <string>

namespace
{
std::once_flag ssl_initialized;
SSL_CTX* ssl_ctx { nullptr };
} // namespace

namespace exios
{

auto ssl_context() noexcept -> SSL_CTX*
{
    EXIOS_EXPECT(ssl_ctx != nullptr);
    return ssl_ctx;
}

auto throw_ssl_error() -> void
{
    auto const e = ERR_get_error();

    std::string err;
    err.resize(512);
    ERR_error_string_n(e, err.data(), err.size());

    throw std::runtime_error { err };
}

auto throw_ssl_error(SSL* ssl, int n) -> void
{
    EXIOS_EXPECT(ssl != nullptr);
    auto const e = SSL_get_error(ssl, n);

    std::string err;
    err.resize(512);
    ERR_error_string_n(e, err.data(), err.size());

    throw std::runtime_error { err };
}

auto initialize_ssl() -> void
{
    std::call_once(ssl_initialized, [] {
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
#if OPENSSL_VERSION_MAJOR < 3
        ERR_load_BIO_strings(); // deprecated since OpenSSL 3.0
#endif
        ERR_load_crypto_strings();

        if (ssl_ctx = SSL_CTX_new(TLS_method()); ssl_ctx == nullptr) {
            throw_ssl_error();
        }

        SSL_CTX_set_options(ssl_ctx,
                            SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    });
}

} // namespace exios
