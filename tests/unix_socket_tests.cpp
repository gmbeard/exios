#include "exios/buffer_view.hpp"
#include "exios/context_thread.hpp"
#include "exios/exios.hpp"
#include "exios/message.hpp"
#include "exios/unix_socket.hpp"
#include "testing.hpp"
#include <fcntl.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std::string_view_literals;

auto should_construct_unix_socket() -> void
{
    exios::ContextThread thread;
    exios::UnixSocket socket { thread };
}

auto should_construct_unix_socket_acceptor() -> void
{
    std::string_view const name = "test";

    exios::ContextThread thread;
    exios::UnixSocketAcceptor socket { thread, name };

    std::cerr << "Acceptor name: " << socket.name() << '\n';
    EXPECT(socket.name() == name);
}

auto should_connect_and_accept() -> void
{
    exios::ContextThread thread;
    exios::UnixSocketAcceptor acceptor { thread, "test"sv };

    exios::UnixSocket client { thread };
    exios::UnixSocket target { thread };

    acceptor.accept(target, [](auto const& result) { EXPECT(result); });

    client.connect("test"sv, [](auto const& result) {
        if (!result)
            std::cerr << result.error().message() << '\n';
        EXPECT(result);
    });

    static_cast<void>(thread.run());
}

auto should_send_and_receive() -> void
{
    struct SenderPeer
    {
        struct OnConnect
        {
        };
        struct OnSend
        {
        };
        exios::UnixSocket& socket;
        exios::ConstBufferView message;

        auto initiate(std::string_view connect_to) && -> void
        {
            socket.connect(connect_to,
                           std::bind(std::move(*this),
                                     OnConnect {},
                                     std::placeholders::_1));
        }

        auto operator()(OnConnect, exios::ConnectResult result) -> void
        {
            EXPECT(result);

            socket.write(
                message,
                std::bind(std::move(*this), OnSend {}, std::placeholders::_1));
        }

        auto operator()(OnSend, exios::IoResult result) -> void
        {
            EXPECT(result);
        }
    };

    struct ReceiverPeer
    {
        struct OnAccept
        {
        };
        struct OnReceive
        {
        };
        exios::UnixSocketAcceptor& acceptor;
        exios::UnixSocket& socket;
        exios::BufferView message;

        auto initiate() && -> void
        {
            acceptor.accept(socket,
                            std::bind(std::move(*this),
                                      OnAccept {},
                                      std::placeholders::_1));
        }

        auto operator()(OnAccept, exios::Result<std::error_code> result) -> void
        {
            EXPECT(result);
            socket.read(message,
                        std::bind(std::move(*this),
                                  OnReceive {},
                                  std::placeholders::_1));
        }

        auto operator()(OnReceive, exios::IoResult result) -> void
        {
            EXPECT(result);
        }
    };

    exios::ContextThread thread;
    exios::UnixSocketAcceptor acceptor { thread, "test"sv };
    exios::UnixSocket send_peer { thread };
    exios::UnixSocket receive_peer { thread };

    std::string message_to_write { "Hello" };
    std::string message_to_receive(message_to_write.size(), '\0');

    SenderPeer(send_peer,
               exios::ConstBufferView { message_to_write.data(),
                                        message_to_write.size() })
        .initiate(acceptor.name());

    ReceiverPeer(acceptor,
                 receive_peer,
                 exios::BufferView { message_to_receive.data(),
                                     message_to_receive.size() })
        .initiate();

    static_cast<void>(thread.run());
    EXPECT(message_to_receive == "Hello");
}

auto write_to_files_and_send(std::string_view content,
                             std::string_view socket_name) -> void
{
    struct SenderPeer
    {
        struct OnConnect
        {
        };
        struct OnSend
        {
        };
        exios::UnixSocket& socket;
        exios::ConstBufferView data_buffer;
        exios::ConstBufferView control_buffer;

        auto initiate(std::string_view connect_to) && -> void
        {
            socket.connect(connect_to,
                           std::bind(std::move(*this),
                                     OnConnect {},
                                     std::placeholders::_1));
        }

        auto operator()(OnConnect, exios::ConnectResult result) -> void
        {
            EXPECT(result);

            socket.send_message(
                data_buffer,
                control_buffer,
                std::bind(std::move(*this), OnSend {}, std::placeholders::_1));
        }

        auto operator()(OnSend, exios::IoResult result) -> void
        {
            if (!result) {
                std::cerr << "SenderPeer error: " << result.error().message()
                          << '\n';
            }
            EXPECT(result);
        }
    };

    exios::ContextThread thread;
    exios::UnixSocket socket { thread };

    auto const* tmp_dir = std::getenv("TMP");
    if (!tmp_dir)
        tmp_dir = "/tmp";

    auto const path = std::filesystem::path { tmp_dir } / "test.txt";

    auto fd = ::open(path.c_str(), O_CREAT | O_RDWR, 0644);
    EXIOS_SCOPE_GUARD([&] { ::close(fd); });

    if (fd < 0)
        std::cerr << std::error_code { errno, std::system_category() }.message()
                  << '\n';

    EXPECT(fd >= 0);
    ssize_t len = static_cast<ssize_t>(content.size());
    auto n = ::write(fd, content.data(), len);
    EXPECT(n == len);
    ::fsync(fd);

    std::size_t num_fds = 1;

    auto sender = SenderPeer(
        socket,
        exios::ConstBufferView { .data =
                                     reinterpret_cast<void const*>(&num_fds),
                                 .size = sizeof(num_fds) },
        exios::ConstBufferView { .data = reinterpret_cast<void const*>(&fd),
                                 .size = sizeof(fd) });

    std::move(sender).initiate(socket_name);
    static_cast<void>(thread.run());
}

auto should_transfer_file_descriptors() -> void
{
    struct ReceiverPeer
    {
        struct OnAccept
        {
        };
        struct OnReceive
        {
        };
        exios::UnixSocketAcceptor& acceptor;
        exios::UnixSocket& socket;
        exios::BufferView data_buffer;
        exios::BufferView control_buffer;
        std::pair<std::size_t, std::size_t>& sizes;

        auto initiate() && -> void
        {
            acceptor.accept(socket,
                            std::bind(std::move(*this),
                                      OnAccept {},
                                      std::placeholders::_1));
        }

        auto operator()(OnAccept, exios::Result<std::error_code> result) -> void
        {
            EXPECT(result);
            socket.receive_message(data_buffer,
                                   control_buffer,
                                   std::bind(std::move(*this),
                                             OnReceive {},
                                             std::placeholders::_1));
        }

        auto operator()(OnReceive, exios::ReceiveMessageManagedResult result)
            -> void
        {
            if (!result) {
                std::cerr << "OnReceive error: " << result.error().message()
                          << '\n';
            }
            EXPECT(result);
            sizes = result.value();
        }
    };

    exios::ContextThread thread;
    exios::UnixSocketAcceptor acceptor { thread, "test"sv };
    exios::UnixSocket socket { thread };

    std::array<std::size_t, 1> num_fds;
    std::array<int, 1> fds;
    auto sizes = std::make_pair(0ul, 0ul);

    ReceiverPeer(acceptor,
                 socket,
                 exios::buffer_view(num_fds),
                 exios::buffer_view(fds),
                 sizes)
        .initiate();

    auto pid = ::fork();
    EXPECT(pid >= 0);

    if (pid == 0) {
        write_to_files_and_send("test"sv, acceptor.name());
        return;
    }

    static_cast<void>(thread.run());
    ::waitpid(pid, nullptr, 0);

    auto const& [data_bytes_received, control_bytes_received] = sizes;

    EXPECT(data_bytes_received >= sizeof(num_fds[0]));
    EXPECT(control_bytes_received >= sizeof(fds[0]));
    EXPECT(num_fds[0] == 1);

    EXIOS_SCOPE_GUARD([&] { ::close(fds[0]); });
    ::lseek(fds[0], 0, SEEK_SET);
    std::string content;

    while (true) {
        char buffer[16] = {};
        auto num_bytes = ::read(fds[0], &buffer, sizeof(buffer));
        EXPECT(num_bytes >= 0);
        std::copy_n(std::begin(buffer), num_bytes, std::back_inserter(content));
        if (!num_bytes)
            break;
    }

    EXPECT(content == "test");
}

auto main() -> int
{
    return testing::run({ TEST(should_construct_unix_socket),
                          TEST(should_construct_unix_socket_acceptor),
                          TEST(should_connect_and_accept),
                          TEST(should_send_and_receive),
                          TEST(should_transfer_file_descriptors) });
}
