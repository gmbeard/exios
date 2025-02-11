#include "exios/buffer_view.hpp"
#include "exios/context_thread.hpp"
#include "exios/exios.hpp"
#include "exios/io.hpp"
#include "exios/udp_socket.hpp"
#include "exios/utils.hpp"
#include "testing.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <thread>

auto should_bind_socket() -> void
{
    exios::ContextThread ctx;
    exios::UdpSocket socket { ctx };

    socket.bind(1900);
}

auto should_send_and_receive() -> void
{
    exios::ContextThread receiver_context, sender_context;
    exios::UdpSocket receiver { receiver_context };
    exios::UdpSocket sender { sender_context };
    std::vector<std::byte> received_data(1024);
    receiver.bind(1900);
    sockaddr_in source_address {};

    std::thread receiver_thread { [&] {
        exios::BufferView buf { received_data.data(), received_data.size() };
        receiver.receive_from(buf, [&](exios::ReceiveFromResult result) {
            EXPECT(result);
            auto const [len, addr] = result.value();
            source_address = addr;
            received_data.resize(len);
        });
        static_cast<void>(receiver_context.run());
    } };

    constexpr std::string_view data = "Hello, World!";
    exios::ConstBufferView buf { data.data(), data.size() };

    sender.send_to(
        buf, "0.0.0.0", 1900, [](exios::IoResult result) { EXPECT(result); });

    static_cast<void>(sender_context.run());
    receiver_thread.join();

    EXPECT(received_data.size() == data.size());
    std::string_view received_message {
        reinterpret_cast<char const*>(received_data.data()),
        static_cast<std::size_t>(received_data.size())
    };
    EXPECT(received_message == data);

    auto const ip = exios::reverse_byte_order(source_address.sin_addr.s_addr);

    std::fprintf(stderr,
                 "Source address: %u.%u.%u.%u\n",
                 static_cast<std::uint32_t>(ip >> 24 & 0xff),
                 static_cast<std::uint32_t>(ip >> 16 & 0xff),
                 static_cast<std::uint32_t>(ip >> 8 & 0xff),
                 static_cast<std::uint32_t>(ip & 0xff));

    std::fprintf(stderr,
                 "Source port: %u\n",
                 static_cast<std::uint32_t>(
                     exios::reverse_byte_order(source_address.sin_port)));
}

auto should_send_and_receive_bound_and_connected() -> void
{
    std::uint16_t const port = 1234;

    exios::ContextThread receiver_context, sender_context;
    exios::UdpSocket receiver { receiver_context };
    exios::UdpSocket sender { sender_context };
    std::vector<std::byte> received_data(1024);
    receiver.bind(port);

    std::thread receiver_thread { [&] {
        exios::BufferView buf { received_data.data(), received_data.size() };
        receiver.read(buf, [&](exios::IoResult result) {
            EXPECT(result);
            auto const len = result.value();
            received_data.resize(len);
        });
        static_cast<void>(receiver_context.run());
    } };

    constexpr std::string_view data = "Hello, World!";

    sender.connect("0.0.0.0", port, [&](exios::ConnectResult connect_result) {
        EXPECT(connect_result);

        exios::ConstBufferView buf { data.data(), data.size() };

        sender.write(buf, [](exios::IoResult result) { EXPECT(result); });
    });

    static_cast<void>(sender_context.run());
    receiver_thread.join();

    EXPECT(received_data.size() == data.size());
    std::string_view received_message {
        reinterpret_cast<char const*>(received_data.data()),
        static_cast<std::size_t>(received_data.size())
    };
    EXPECT(received_message == data);
}

auto main() -> int
{
    return testing::run({ TEST(should_bind_socket),
                          TEST(should_send_and_receive),
                          TEST(should_send_and_receive_bound_and_connected) });
}
