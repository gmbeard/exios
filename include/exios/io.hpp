#ifndef EXIOS_IO_HPP_INCLUDED
#define EXIOS_IO_HPP_INCLUDED

#include "exios/buffer_view.hpp"
#include "exios/contracts.hpp"
#include "exios/result.hpp"
#include <cinttypes>
#include <optional>
#include <system_error>

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

constexpr WriteOperation write_operation {};
constexpr ReadOperation read_operation {};
constexpr TimerExpiryOrEventOperation timer_expiry_operation {};
constexpr TimerExpiryOrEventOperation event_read_operation {};
constexpr EventWriteOperation event_write_operation {};

using IoResult = Result<std::size_t, std::error_code>;
using TimerOrEventIoResult = Result<std::uint64_t, std::error_code>;

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

template <typename Tag>
using IoOperationType = typename IoOperation<Tag>::type;

} // namespace exios

#endif // EXIOS_IO_HPP_INCLUDED
