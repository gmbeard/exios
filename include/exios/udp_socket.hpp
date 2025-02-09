#ifndef EXIOS_UDP_SOCKET_HPP_INCLUDED
#define EXIOS_UDP_SOCKET_HPP_INCLUDED

#include "exios/buffer_view.hpp"
#include "exios/io.hpp"
#include "exios/io_object.hpp"
#include "exios/utils.hpp"
#include "exios/work.hpp"
#include <cstdint>
#include <netinet/in.h>
namespace exios
{

struct UdpSocket : IoObject
{
    explicit UdpSocket(Context const&);

    /**
     * Completes with:
     *   Result<std::error_code>
     */
    template <typename Completion>
    auto connect(std::string_view address,
                 std::uint16_t port,
                 Completion&& completion) -> void
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

    auto bind(std::uint16_t port, std::string_view address = "0.0.0.0") -> void;

    /**
     * Completes with:
     *   Result<std::tuple<std::size_t, sockaddr_in>, std::error_code>
     */
    template <typename Completion>
    auto receive_from(BufferView buffer, Completion&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op = make_async_io_operation(
            net_receive_from_operation,
            wrap_work(std::move(completion), get_context()),
            alloc,
            ctx_,
            fd_.value(),
            buffer);

        schedule_io(op);
    }

    /**
     * Completes with:
     *   Result<std::size_t, std::error_code>
     */
    template <typename Completion>
    auto send_to(ConstBufferView buffer,
                 std::string_view address,
                 std::int16_t port,
                 Completion&& completion) -> void
    {
        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = reverse_byte_order(parse_ipv4(address));
        addr.sin_port = reverse_byte_order(port);

        auto const alloc = select_allocator(completion);

        auto* op = make_async_io_operation(
            net_send_to_operation,
            wrap_work(std::move(completion), get_context()),
            alloc,
            ctx_,
            fd_.value(),
            buffer,
            addr);

        schedule_io(op);
    }

    /**
     * \brief Asynchronously reads from the I/O object
     *
     * Upon completion, `completion(arg)` will be invoked. `buffer` will contain
     * `arg.value()` bytes, unless `!arg`.
     *
     * **NOTE: This member can only be used after a successful call to
     * `connect()`**
     *
     * \param buffer The buffer into which to receive the data. *NOTE*: The
     * memory to which this points to must live until `completion` is called.
     * \param completion A callable that accepts a ::exios::IoResult
     */
    template <typename F>
    auto read(BufferView buffer, F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op = make_async_io_operation(
            read_operation,
            wrap_work(std::move(completion), get_context()),
            alloc,
            ctx_,
            fd_.value(),
            buffer);

        schedule_io(op);
    }

    /**
     * \brief Asynchronously writes to the I/O object
     *
     * Upon completion, `completion(arg)` will be invoked. `buffer` will contain
     * `arg.value()` bytes, unless `!arg`.
     *
     * **NOTE: This member can only be used after a successful call to
     * `connect()`**
     *
     * \param buffer The buffer from which to send the data. *NOTE*: The
     * memory to which this points to must live until `completion` is called.
     * \param completion A callable that accepts a ::exios::IoResult
     */
    template <typename F>
    auto write(ConstBufferView buffer, F&& completion) -> void
    {
        auto const alloc = select_allocator(completion);

        auto* op = make_async_io_operation(
            write_operation,
            wrap_work(std::move(completion), get_context()),
            alloc,
            ctx_,
            fd_.value(),
            buffer);

        schedule_io(op);
    }
};

} // namespace exios

#endif // EXIOS_UDP_SOCKET_HPP_INCLUDED
