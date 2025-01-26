#include "exios/exios.hpp"
#include "testing.hpp"

auto should_create_acceptor_on_localhost() -> void
{
    exios::ContextThread context;
    exios::TcpSocketAcceptor acceptor { context, 8080, "127.0.0.1" };
    exios::TcpSocket socket { context };

    bool accepted = false;
    acceptor.accept(socket,
                    [&](auto result) { accepted = !result.is_error_value(); });

    static_cast<void>(context.run());

    EXPECT(accepted);
}

auto main() -> int
{
    return testing::run({ TEST(should_create_acceptor_on_localhost) });
}
