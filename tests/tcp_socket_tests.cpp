#include "exios/exios.hpp"
#include "exios/io.hpp"
#include "testing.hpp"
#include <cstdio>
#include <thread>

auto should_create_acceptor_on_localhost() -> void
{
    exios::ContextThread accept_context, connect_context;
    exios::TcpSocketAcceptor acceptor { accept_context, 8080, "127.0.0.1" };
    exios::TcpSocket connector { connect_context };
    exios::TcpSocket socket { accept_context };

    bool accepted = false;
    acceptor.accept(socket,
                    [&](auto result) { accepted = !result.is_error_value(); });

    std::thread connector_thread { [&] {
        connector.connect("0.0.0.0", 8080, [&](exios::ConnectResult result) {
            EXPECT(result);
        });
        static_cast<void>(connect_context.run());
    } };

    static_cast<void>(accept_context.run());
    connector_thread.join();

    EXPECT(accepted);
}

auto main() -> int
{
    return testing::run({ TEST(should_create_acceptor_on_localhost) });
}
