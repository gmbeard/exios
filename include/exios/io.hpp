#ifndef EXIOS_IO_HPP_INCLUDED
#define EXIOS_IO_HPP_INCLUDED

#include "exios/buffer_view.hpp"
#include "exios/contracts.hpp"
#include "exios/result.hpp"
#include <cinttypes>
#include <optional>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <system_error>
#include <tuple>

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

using IoResult = Result<std::size_t, std::error_code>;
using ConnectResult = Result<std::error_code>;
using AcceptResult = Result<int, std::error_code>;
using TimerOrEventIoResult = Result<std::uint64_t, std::error_code>;
using SignalResult = Result<signalfd_siginfo, std::error_code>;
using ReceiveMessageResult =
    Result<std::pair<std::size_t, msghdr>, std::error_code>;

auto perform_read(int fd, BufferView buffer) noexcept -> IoResult;
auto perform_write(int fd, ConstBufferView buffer) noexcept -> IoResult;
auto perform_timer_or_event_read(int fd) noexcept -> TimerOrEventIoResult;

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
struct IoOperation<UnixAcceptOperation>
{
    using type = UnixAccept;
};

template <>
struct IoOperation<SendMessageOperation>
{
    using type = SendMessage;
};

template <>
struct IoOperation<ReceiveMessageOperation>
{
    using type = ReceiveMessage;
};

template <typename Tag>
using IoOperationType = typename IoOperation<Tag>::type;

} // namespace exios

#endif // EXIOS_IO_HPP_INCLUDED
