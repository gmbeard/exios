#include "exios/unix_socket.hpp"
#include "exios/contracts.hpp"
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <system_error>

namespace exios
{

UnixSocket::UnixSocket(Context const& ctx)
    : IoObject {
        ctx, ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)
    }
{
    if (fd_.value() < 0)
        throw std::system_error { errno, std::system_category() };
}

UnixSocket::UnixSocket(Context const& ctx, int fd) noexcept
    : IoObject { ctx, fd }
{
}

UnixSocketAcceptor::UnixSocketAcceptor(Context const& ctx,
                                       std::string_view abstract_name)
    : IoObject { ctx,
                 ::socket(
                     AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0) }
    , addr_ {}
{
    EXIOS_EXPECT(abstract_name.size() < sizeof(addr_.sun_path));

    if (fd_.value() < 0)
        throw std::system_error { errno, std::system_category() };

    addr_.sun_family = AF_UNIX;
    addr_.sun_path[0] = '\0';
    abstract_name.copy(addr_.sun_path + 1, abstract_name.size());

    if (auto const result = ::bind(fd_.value(),
                                   reinterpret_cast<sockaddr const*>(&addr_),
                                   sizeof(addr_));
        result < 0)
        throw std::system_error { errno, std::system_category() };

    if (auto const result = ::listen(fd_.value(), SOMAXCONN); result < 0)
        throw std::system_error { errno, std::system_category() };
}

auto UnixSocketAcceptor::name() const noexcept -> std::string_view
{
    return { addr_.sun_path + 1, sizeof(addr_.sun_path) - 1 };
}

} // namespace exios
