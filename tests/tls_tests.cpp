#include "exios/buffer_view.hpp"
#include "exios/exios.hpp"
#include "exios/io.hpp"
#include "testing.hpp"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <iostream>
#include <string_view>
#include <system_error>
#include <vector>

auto should_connect() -> void
{
    exios::ContextThread ctx;
    exios::TlsTcpSocket socket { ctx, exios::SSLMode::client };

    /* NOTE:
     * Connect to local running s_server...
     */
    socket.connect("127.0.0.1", 8443, [&](auto connect_result) {
        EXPECT(connect_result);

        socket.shutdown([](auto shutdown_result) { EXPECT(shutdown_result); });
    });

    static_cast<void>(ctx.run());
}

auto should_send() -> void
{
    constexpr std::string_view kData =
        "GET / HTTP/1.1\r\n"
        "Host: localhost:8443\r\n"
        "\r\n";

    std::vector<char> receive_buffer(4096);

    exios::ContextThread ctx;
    exios::TlsTcpSocket socket { ctx, exios::SSLMode::client };

    struct Op
    {
        exios::TlsTcpSocket& socket;
        std::string_view data;
        std::vector<char>& receive_buffer;
        std::size_t sent { 0 };

        auto operator()() -> void
        {
            socket.connect("127.0.0.1",
                           8443,
                           [self = std::move(*this)](auto result) mutable {
                               self.on_connect(std::move(result));
                           });
        }

        auto on_connect(exios::ConnectResult result) -> void
        {
            if (!result) {
                std::fprintf(stderr,
                             "on_read error: %s\n",
                             result.error().message().c_str());
            }

            EXPECT(result);
            socket.write(exios::const_buffer_view(data),
                         [self = std::move(*this)](auto write_result) mutable {
                             self.on_write(std::move(write_result));
                         });
        }

        auto on_write(exios::IoResult result) -> void
        {
            EXPECT(result);
            sent += result.value();
            if (sent < data.size()) {
                socket.write(
                    exios::const_buffer_view(data.substr(sent)),
                    [self = std::move(*this)](auto write_result) mutable {
                        self.on_write(std::move(write_result));
                    });
                return;
            }

            socket.read(exios::buffer_view(receive_buffer),
                        [self = std::move(*this)](auto read_result) mutable {
                            self.on_read(std::move(read_result));
                        });
        }

        auto on_read(exios::IoResult result) -> void
        {
            if (!result) {
                std::fprintf(stderr,
                             "on_read error: %s\n",
                             result.error().message().c_str());
            }

            EXPECT(result);

            if (result.value() == 0)
                return;

            std::copy(receive_buffer.begin(),
                      std::next(receive_buffer.begin(), result.value()),
                      std::ostreambuf_iterator<char>(std::cerr));

            socket.read(exios::buffer_view(receive_buffer),
                        [self = std::move(*this)](auto read_result) mutable {
                            self.on_read(std::move(read_result));
                        });
        }
    };

    Op(socket, kData, receive_buffer)();

    static_cast<void>(ctx.run());
}

auto main() -> int
{
    return testing::run({ TEST(should_connect), TEST(should_send) });
}
