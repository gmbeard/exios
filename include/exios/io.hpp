#ifndef EXIOS_IO_HPP_INCLUDED
#define EXIOS_IO_HPP_INCLUDED

#include "exios/buffer_view.hpp"
#include "exios/contracts.hpp"
#include "exios/message.hpp"
#include "exios/result.hpp"
#include <cinttypes>
#include <netinet/in.h>
#include <optional>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <system_error>
#include <tuple>
#include <type_traits>

namespace exios
{
struct WriteOperation
{
};
struct ReadOperation
{
};

struct TimerExpiryOrEventOperation
{
};

struct EventWriteOperation
{
};

struct SignalReadOperation
{
};

struct UnixConnectOperation
{
};

struct UnixAcceptOperation
{
};

struct ReceiveMessageOperation
{
};

struct SendMessageOperation
{
};

template <typename Allocator>
struct SendMessageManagedOperation
{
    using allocator = std::remove_cvref_t<Allocator>;
};

template <typename Allocator>
struct ReceiveMessageManagedOperation
{
    using allocator = std::remove_cvref_t<Allocator>;
};

struct NetConnectOperation
{
};

struct NetSendToOperation
{
};

struct NetReceiveFromOperation
{
};

constexpr WriteOperation write_operation {};
constexpr ReadOperation read_operation {};
constexpr TimerExpiryOrEventOperation timer_expiry_operation {};
constexpr TimerExpiryOrEventOperation event_read_operation {};
constexpr EventWriteOperation event_write_operation {};
constexpr SignalReadOperation signal_read_operation {};
constexpr UnixConnectOperation unix_connect_operation {};
constexpr UnixAcceptOperation unix_accept_operation {};
constexpr SendMessageOperation send_message_operation {};
constexpr ReceiveMessageOperation receive_message_operation {};
constexpr NetConnectOperation net_connect_operation {};
constexpr NetSendToOperation net_send_to_operation {};
constexpr NetReceiveFromOperation net_receive_from_operation {};

template <typename Allocator>
constexpr auto send_message_managed_operation() noexcept
    -> SendMessageManagedOperation<Allocator>
{
    return {};
}

template <typename Allocator>
constexpr auto receive_message_managed_operation() noexcept
    -> ReceiveMessageManagedOperation<Allocator>
{
    return {};
}

using IoResult = Result<std::size_t, std::error_code>;
using ReceiveMessageManagedResult =
    Result<std::pair<std::size_t, std::size_t>, std::error_code>;
using ConnectResult = Result<std::error_code>;
using AcceptResult = Result<int, std::error_code>;
using TimerOrEventIoResult = Result<std::uint64_t, std::error_code>;
using SignalResult = Result<signalfd_siginfo, std::error_code>;
using ReceiveMessageResult =
    Result<std::pair<std::size_t, msghdr>, std::error_code>;
using ReceiveFromResult =
    Result<std::tuple<std::size_t, sockaddr_in>, std::error_code>;

auto perform_read(int fd, BufferView buffer) noexcept -> IoResult;
auto perform_write(int fd, ConstBufferView buffer) noexcept -> IoResult;
auto perform_timer_or_event_read(int fd) noexcept -> TimerOrEventIoResult;
auto perform_send(int fd, msghdr const& buf) noexcept -> IoResult;
auto perform_receive(int fd, msghdr& buf) noexcept -> ReceiveMessageResult;

struct IoOpBase
{
    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

    auto cancel() noexcept -> void;

protected:
    auto set_result(IoResult&& r) noexcept -> void;

private:
    std::optional<IoResult> result_;
};

struct IoRead : IoOpBase
{
    IoRead(BufferView) noexcept;

    static constexpr auto is_readable = std::true_type {};
    auto io(int fd) noexcept -> bool;

private:
    BufferView buffer_;
};

struct IoWrite : IoOpBase
{
    IoWrite(ConstBufferView) noexcept;

    static constexpr auto is_readable = std::false_type {};
    auto io(int fd) noexcept -> bool;

private:
    ConstBufferView buffer_;
};

struct ReceiveMessage
{
    explicit ReceiveMessage(msghdr msg) noexcept;
    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::true_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<ReceiveMessageResult> result_;
    msghdr msg_;
};

struct SendMessage
{
    explicit SendMessage(msghdr msg) noexcept;
    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::false_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<IoResult> result_;
    msghdr msg_;
};

template <typename Allocator>
struct SendMessageManaged
{
    explicit SendMessageManaged(outgoing_message_base<Allocator> msg) noexcept
        : msg_ { std::move(msg) }
    {
    }

    auto io(int fd) noexcept -> bool
    {
        EXIOS_EXPECT(!result_);
        msghdr tmp;
        try {
            tmp = message_view(msg_);
        }
        catch (...) {
            result_.emplace(result_error(
                std::error_code { static_cast<int>(std::errc::no_buffer_space),
                                  std::system_category() }));
            return true;
        }

        std::array<iovec, 1> iov { iovec {
            const_cast<void*>(msg_.data_buffer().data),
            msg_.data_buffer().size } };
        tmp.msg_iov = iov.data();
        tmp.msg_iovlen = iov.size();

        auto r = perform_send(fd, tmp);
        if (!r && (r.error() == std::errc::operation_in_progress ||
                   r.error() == std::errc::operation_would_block))
            return false;

        result_.emplace(std::move(r));
        return true;
    }

    auto cancel() noexcept -> void
    {
        result_.emplace(
            result_error(std::make_error_code(std::errc::operation_canceled)));
    }

    static constexpr auto is_readable = std::false_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<IoResult> result_;
    outgoing_message_base<Allocator> msg_;
};

template <typename Allocator>
struct ReceiveMessageManaged
{
    explicit ReceiveMessageManaged(
        incoming_message_base<Allocator> msg) noexcept
        : msg_ { std::move(msg) }
    {
    }

    auto io(int fd) noexcept -> bool
    {
        EXIOS_EXPECT(!result_);
        msghdr tmp;
        try {
            tmp = message_view(msg_);
        }
        catch (...) {
            result_.emplace(result_error(
                std::error_code { static_cast<int>(std::errc::no_buffer_space),
                                  std::system_category() }));
            return true;
        }

        std::array<iovec, 1> iov { iovec {
            const_cast<void*>(msg_.data_buffer().data),
            msg_.data_buffer().size } };
        tmp.msg_iov = iov.data();
        tmp.msg_iovlen = iov.size();

        auto r = perform_receive(fd, tmp);
        if (!r && (r.error() == std::errc::operation_in_progress ||
                   r.error() == std::errc::operation_would_block))
            return false;

        if (!r) {
            result_.emplace(result_error(r.error()));
            return true;
        }

        std::size_t control_bytes_copied = 0;

        if (msg_.control_buffer().size) {
            auto control_buffer = msg_.control_buffer();
            cmsghdr* cmsg = CMSG_FIRSTHDR(&tmp);
            EXIOS_EXPECT(cmsg);
            std::size_t const bytes_to_copy =
                std::min(cmsg->cmsg_len, control_buffer.size);

            std::copy_n(reinterpret_cast<std::uint8_t const*>(CMSG_DATA(cmsg)),
                        bytes_to_copy,
                        reinterpret_cast<std::uint8_t*>(control_buffer.data));

            control_bytes_copied = bytes_to_copy;
        }

        result_.emplace(result_ok(
            std::make_pair(std::get<0>(r.value()), control_bytes_copied)));
        return true;
    }

    auto cancel() noexcept -> void
    {
        result_.emplace(
            result_error(std::make_error_code(std::errc::operation_canceled)));
    }

    static constexpr auto is_readable = std::false_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<ReceiveMessageManagedResult> result_;
    incoming_message_base<Allocator> msg_;
};

struct NetSendTo
{
    explicit NetSendTo(ConstBufferView buffer, sockaddr_in addr) noexcept;
    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::false_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<IoResult> result_;
    ConstBufferView buffer_;
    sockaddr_in addr_;
};

struct NetReceiveFrom
{
    explicit NetReceiveFrom(BufferView buffer) noexcept;
    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::true_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<ReceiveFromResult> result_;
    BufferView buffer_;
};

struct UnixConnect
{
    explicit UnixConnect(std::string_view name) noexcept;

    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::false_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<ConnectResult> result_;
    sockaddr_un addr_;
    std::size_t name_length_;
};

struct NetConnect
{
    explicit NetConnect(sockaddr_in addr) noexcept;

    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::false_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<ConnectResult> result_;
    sockaddr_in addr_;
};

struct UnixAccept
{
    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::true_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<AcceptResult> result_;
};

struct TimerExpiryOrEvent
{
    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::true_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<TimerOrEventIoResult> result_;
};

struct EventWrite
{
    explicit EventWrite(std::optional<std::uint64_t> value_to_write) noexcept;
    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::false_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<std::uint64_t> value_to_write_;
    std::optional<IoResult> result_;
};

struct SignalRead
{
    auto io(int fd) noexcept -> bool;
    auto cancel() noexcept -> void;

    static constexpr auto is_readable = std::true_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<SignalResult> result_;
};

template <typename Tag>
struct IoOperation;

template <>
struct IoOperation<ReadOperation>
{
    using type = IoRead;
};

template <>
struct IoOperation<WriteOperation>
{
    using type = IoWrite;
};

template <>
struct IoOperation<TimerExpiryOrEventOperation>
{
    using type = TimerExpiryOrEvent;
};

template <>
struct IoOperation<EventWriteOperation>
{
    using type = EventWrite;
};

template <>
struct IoOperation<SignalReadOperation>
{
    using type = SignalRead;
};

template <>
struct IoOperation<UnixConnectOperation>
{
    using type = UnixConnect;
};

template <>
struct IoOperation<NetConnectOperation>
{
    using type = NetConnect;
};

template <>
struct IoOperation<UnixAcceptOperation>
{
    using type = UnixAccept;
};

template <>
struct IoOperation<SendMessageOperation>
{
    using type = SendMessage;
};

template <typename Allocator>
struct IoOperation<SendMessageManagedOperation<Allocator>>
{
    using type = SendMessageManaged<std::remove_cvref_t<Allocator>>;
};

template <typename Allocator>
struct IoOperation<ReceiveMessageManagedOperation<Allocator>>
{
    using type = ReceiveMessageManaged<std::remove_cvref_t<Allocator>>;
};

template <>
struct IoOperation<ReceiveMessageOperation>
{
    using type = ReceiveMessage;
};

template <>
struct IoOperation<NetSendToOperation>
{
    using type = NetSendTo;
};

template <>
struct IoOperation<NetReceiveFromOperation>
{
    using type = NetReceiveFrom;
};

template <typename Tag>
using IoOperationType = typename IoOperation<Tag>::type;

} // namespace exios

#endif // EXIOS_IO_HPP_INCLUDED
