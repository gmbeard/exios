#include "exios/context_thread.hpp"
#include "exios/exios.hpp"
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
    exios::ContextThread thread;
    exios::UnixSocketAcceptor socket { thread, "test"sv };
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
        msghdr message;

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
                message,
                std::bind(std::move(*this), OnSend {}, std::placeholders::_1));
        }

        auto operator()(OnSend, exios::IoResult result) -> void
        {
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

    msghdr msg {};
    char cmsgbuf[CMSG_SPACE(sizeof(int))] = {};
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = CMSG_SPACE(sizeof(int));

    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = fd;
    std::size_t num_fds = 1;
    iovec data { &num_fds, sizeof(std::size_t) };
    msg.msg_iov = &data;
    msg.msg_iovlen = 1;

    SenderPeer(socket, msg).initiate(socket_name);
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
        msghdr& message;

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
            socket.receive_message(message,
                                   std::bind(std::move(*this),
                                             OnReceive {},
                                             std::placeholders::_1));
        }

        auto operator()(OnReceive, exios::ReceiveMessageResult result) -> void
        {
            EXPECT(result);
        }
    };

    exios::ContextThread thread;
    exios::UnixSocketAcceptor acceptor { thread, "test"sv };
    exios::UnixSocket socket { thread };

    msghdr msg {};
    std::size_t num_fds;
    iovec data { &num_fds, sizeof(std::size_t) };
    msg.msg_iov = &data;
    msg.msg_iovlen = 1;
    char cmsgbuf[CMSG_SPACE(sizeof(int))] {};
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    ReceiverPeer(acceptor, socket, msg).initiate();

    auto pid = ::fork();
    EXPECT(pid >= 0);

    if (pid == 0) {
        write_to_files_and_send("test"sv, acceptor.name());
        return;
    }

    static_cast<void>(thread.run());
    ::waitpid(pid, nullptr, 0);

    EXPECT(num_fds == 1);
    EXPECT(msg.msg_controllen == CMSG_SPACE(sizeof(int)));
    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    EXPECT(cmsg);
    EXPECT(cmsg->cmsg_len == CMSG_LEN(sizeof(int)));
    int fd = *reinterpret_cast<int*>(CMSG_DATA(cmsg));

    EXIOS_SCOPE_GUARD([&] { ::close(fd); });
    ::lseek(fd, 0, SEEK_SET);
    std::string content;

    while (true) {
        char buffer[16] = {};
        auto num_bytes = ::read(fd, &buffer, sizeof(buffer));
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
