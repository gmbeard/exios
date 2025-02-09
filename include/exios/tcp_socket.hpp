#ifndef EXIOS_TCP_SOCKET_HPP_INCLUDED
#define EXIOS_TCP_SOCKET_HPP_INCLUDED

#include "exios/context.hpp"
#include "exios/io.hpp"
#include "exios/io_object.hpp"
#include "exios/utils.hpp"
#include <cstdint>
#include <netinet/in.h>
#include <string_view>

namespace exios
{

struct TcpSocket : IoObject
{
    friend struct TcpSocketAcceptor;

    explicit TcpSocket(Context const&);

    template <typename F>
    auto connect(std::string_view address, std::uint16_t port, F&& completion)
        -> void
    {
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = reverse_byte_order(parse_ipv4(address));
        addr.sin_port = reverse_byte_order(port);

        auto const alloc = select_allocator(completion);

        auto* op = make_async_io_operation(
            net_connect_operation,
            wrap_work(std::move(completion), get_context()),
            alloc,
            ctx_,
            fd_.value(),
            addr);

        schedule_io(op);
    }

    /**
     * \brief Asynchronously reads from the I/O object
     *
     * Upon completion, `completion(arg)` will be invoked. `buffer` will contain
     * `arg.value()` bytes, unless `!arg`.
     *
     * \param buffer The buffer into which to receive the data. *NOTE*: The
     * memory to which this points to must live until `completion` is called.
     * \param completion A callable that accepts a ::exios::IoResult
     */
    template <typename F>
    auto read(BufferView buffer, F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op =
            make_async_io_operation(read_operation,
                                    wrap_work(std::move(completion), ctx_),
                                    alloc,
                                    ctx_,
                                    fd_.value(),
                                    buffer);

        schedule_io(op);
    }

    /**
     * \brief Asynchronously receives a msghdr object
     *
     * \param msg A ::msghdr object to hold the received message. *NOTE*: All
     * memory to which refers must live until `completion` is called.
     * \param completion A callable that accepts a ::exios::ReceiveMessageResult
     */
    template <typename F>
    auto receive_message(msghdr msg, F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op =
            make_async_io_operation(receive_message_operation,
                                    wrap_work(std::move(completion), ctx_),
                                    alloc,
                                    ctx_,
                                    fd_.value(),
                                    msg);

        schedule_io(op);
    }

    template <typename F>
    auto write(ConstBufferView buffer, F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op =
            make_async_io_operation(write_operation,
                                    wrap_work(std::move(completion), ctx_),
                                    alloc,
                                    ctx_,
                                    fd_.value(),
                                    buffer);

        schedule_io(op);
    }

    template <typename F>
    auto send_message(msghdr msg, F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op =
            make_async_io_operation(send_message_operation,
                                    wrap_work(std::move(completion), ctx_),
                                    alloc,
                                    ctx_,
                                    fd_.value(),
                                    msg);

        schedule_io(op);
    }

private:
    explicit TcpSocket(Context const&, int /*fd*/) noexcept;
};

struct TcpSocketAcceptor : IoObject
{
    TcpSocketAcceptor(Context const& context, std::uint16_t port);

    TcpSocketAcceptor(Context const& context,
                      std::uint16_t port,
                      std::string_view address);

    auto port() const noexcept -> std::uint16_t;
    auto address() const noexcept -> std::string_view;

    template <typename F>
    auto accept(TcpSocket& target, F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op = make_async_io_operation(
            unix_accept_operation,
            wrap_work(
                [ctx = ctx_, completion = std::move(completion), &target](
                    auto&& result) mutable {
                    if (!result)
                        std::move(completion)(Result<std::error_code> {
                            result_error(std::move(result).error()) });
                    else {
                        target = TcpSocket { ctx, std::move(result).value() };
                        std::move(completion)(Result<std::error_code> {});
                    }
                },
                ctx_),
            alloc,
            ctx_,
            fd_.value());

        schedule_io(op);
    }

private:
    sockaddr_in addr_;
};

} // namespace exios

#endif // EXIOS_TCP_SOCKET_HPP_INCLUDED
