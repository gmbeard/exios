#include "exios/io.hpp"
#include "exios/result.hpp"
#include <cerrno>
#include <cstddef>
#include <errno.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>

namespace exios
{
auto perform_read(int fd, BufferView buf) noexcept -> IoResult
{
    auto const result = ::read(fd, buf.data, buf.size);
    if (result < 0)
        return result_error(std::error_code { errno, std::system_category() });

    return result_ok(static_cast<std::size_t>(result));
}

auto perform_write(int fd, ConstBufferView buf) noexcept -> IoResult
{
    auto const result = ::write(fd, buf.data, buf.size);
    if (result < 0)
        return result_error(std::error_code { errno, std::system_category() });

    return result_ok(static_cast<std::size_t>(result));
}

auto perform_receive(int fd, msghdr& buf) noexcept -> ReceiveMessageResult
{
    auto const result = ::recvmsg(fd, &buf, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (result < 0)
        return result_error(std::error_code { errno, std::system_category() });

    return result_ok(std::make_pair(static_cast<std::size_t>(result), buf));
}

auto perform_send(int fd, msghdr const& buf) noexcept -> IoResult
{
    auto const result = ::sendmsg(fd, &buf, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (result < 0)
        return result_error(std::error_code { errno, std::system_category() });

    return result_ok(static_cast<std::size_t>(result));
}

auto perform_timer_or_event_read(int fd) noexcept -> TimerOrEventIoResult
{
    std::uint64_t val;
    auto const result = ::read(fd, &val, sizeof(val));
    if (result < 0)
        return result_error(std::error_code { errno, std::system_category() });

    return result_ok(val);
}

auto IoOpBase::cancel() noexcept -> void
{
    result_.emplace(
        result_error(std::make_error_code(std::errc::operation_canceled)));
}

auto IoOpBase::set_result(IoResult&& r) noexcept -> void
{
    EXIOS_EXPECT(!result_);
    result_.emplace(std::move(r));
}

IoRead::IoRead(BufferView buffer) noexcept
    : buffer_ { buffer }
{
}

auto IoRead::io(int fd) noexcept -> bool
{
    auto r = perform_read(fd, buffer_);
    if (r.is_error_value() && r.error() == std::errc::operation_would_block)
        return false;

    set_result(std::move(r));
    return true;
}

IoWrite::IoWrite(ConstBufferView buffer) noexcept
    : buffer_ { buffer }
{
}

auto IoWrite::io(int fd) noexcept -> bool
{
    auto r = perform_write(fd, buffer_);
    if (r.is_error_value() && r.error() == std::errc::operation_would_block)
        return false;

    set_result(std::move(r));
    return true;
}

UnixConnect::UnixConnect(std::string_view name) noexcept
    : addr_ {}
{
    EXIOS_EXPECT(name.size() < sizeof(addr_.sun_path));
    addr_.sun_family = AF_UNIX;
    addr_.sun_path[0] = '\0';
    name.copy(addr_.sun_path + 1, name.size());
}

auto UnixConnect::io(int fd) noexcept -> bool
{
    EXIOS_EXPECT(!result_);
    auto const r =
        ::connect(fd, reinterpret_cast<sockaddr const*>(&addr_), sizeof(addr_));
    if (r < 0 && (errno == EAGAIN || errno == EINPROGRESS))
        return false;

    if (r < 0) {
        result_.emplace(
            result_error(std::error_code { errno, std::system_category() }));
    }
    else {
        result_.emplace(ConnectResult {});
    }

    return true;
}

auto UnixConnect::cancel() noexcept -> void
{
    result_.emplace(
        result_error(std::make_error_code(std::errc::operation_canceled)));
}

auto UnixAccept::io(int fd) noexcept -> bool
{
    EXIOS_EXPECT(!result_);
    auto const r = ::accept(fd, nullptr, nullptr);
    if (r < 0 && (errno == EAGAIN || errno == EINPROGRESS))
        return false;

    if (r < 0) {
        result_.emplace(
            result_error(std::error_code { errno, std::system_category() }));
    }
    else {
        result_.emplace(result_ok(r));
    }

    return true;
}

auto UnixAccept::cancel() noexcept -> void
{
    result_.emplace(
        result_error(std::make_error_code(std::errc::operation_canceled)));
}

auto TimerExpiryOrEvent::io(int fd) noexcept -> bool
{
    EXIOS_EXPECT(!result_);
    auto r = perform_timer_or_event_read(fd);
    if (r.is_error_value() && (r.error() == std::errc::operation_would_block ||
                               r.error() == std::errc::operation_in_progress))
        return false;

    result_.emplace(r);
    return true;
}

auto TimerExpiryOrEvent::cancel() noexcept -> void
{
    result_.emplace(
        result_error(std::make_error_code(std::errc::operation_canceled)));
}

EventWrite::EventWrite(std::optional<std::uint64_t> value_to_write) noexcept
    : value_to_write_ { std::move(value_to_write) }
{
}

auto EventWrite::io(int fd) noexcept -> bool
{
    EXIOS_EXPECT(!result_);
    std::uint64_t val = value_to_write_.has_value() ? *value_to_write_ : 1;
    ConstBufferView buffer { .data = &val, .size = sizeof(val) };
    auto r = perform_write(fd, buffer);
    if (r.is_error_value() && (r.error() == std::errc::operation_would_block ||
                               r.error() == std::errc::operation_in_progress))
        return false;

    result_.emplace(r);
    return true;
}

auto EventWrite::cancel() noexcept -> void
{
    result_.emplace(
        result_error(std::make_error_code(std::errc::operation_canceled)));
}

auto SignalRead::io(int fd) noexcept -> bool
{
    EXIOS_EXPECT(!result_);
    signalfd_siginfo val;
    BufferView buffer { .data = &val, .size = sizeof(val) };
    auto r = perform_read(fd, buffer);
    if (r.is_error_value() && (r.error() == std::errc::operation_would_block ||
                               r.error() == std::errc::operation_in_progress))
        return false;

    result_.emplace(result_ok(val));
    return true;
}

auto SignalRead::cancel() noexcept -> void
{
    result_.emplace(
        result_error(std::make_error_code(std::errc::operation_canceled)));
}

SendMessage::SendMessage(msghdr msg) noexcept
    : msg_ { msg }
{
}

auto SendMessage::io(int fd) noexcept -> bool
{
    EXIOS_EXPECT(!result_);
    auto r = perform_send(fd, msg_);
    if (!r && (r.error() == std::errc::operation_in_progress ||
               r.error() == std::errc::operation_would_block))
        return false;

    result_.emplace(std::move(r));
    return true;
}

auto SendMessage::cancel() noexcept -> void
{
    result_.emplace(
        result_error(std::make_error_code(std::errc::operation_canceled)));
}

ReceiveMessage::ReceiveMessage(msghdr msg) noexcept
    : msg_ { msg }
{
}

auto ReceiveMessage::io(int fd) noexcept -> bool
{
    EXIOS_EXPECT(!result_);
    auto r = perform_receive(fd, msg_);
    if (!r && (r.error() == std::errc::operation_in_progress ||
               r.error() == std::errc::operation_would_block))
        return false;

    result_.emplace(std::move(r));
    return true;
}

auto ReceiveMessage::cancel() noexcept -> void
{
    result_.emplace(
        result_error(std::make_error_code(std::errc::operation_canceled)));
}

} // namespace exios
