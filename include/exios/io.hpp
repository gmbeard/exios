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

struct TimerExpiryOperation
{
};

constexpr WriteOperation write_operation {};
constexpr ReadOperation read_operation {};
constexpr TimerExpiryOperation timer_expiry_operation {};

using IoResult = Result<std::size_t, std::error_code>;
using TimerIoResult = Result<std::uint64_t, std::error_code>;

auto perform_read(int fd, BufferView buffer) noexcept -> IoResult;
auto perform_write(int fd, ConstBufferView buffer) noexcept -> IoResult;
auto perform_timer_read(int fd) noexcept -> TimerIoResult;

struct IoOpBase
{
    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

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

struct TimerExpiry
{
    auto io(int fd) noexcept -> bool;

    static constexpr auto is_readable = std::true_type {};

    template <typename F>
    auto dispatch(F&& f) -> void
    {
        EXIOS_EXPECT(result_);
        std::forward<F>(f)(std::move(*result_));
    }

private:
    std::optional<TimerIoResult> result_;
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
struct IoOperation<TimerExpiryOperation>
{
    using type = TimerExpiry;
};

template <typename Tag>
using IoOperationType = typename IoOperation<Tag>::type;

} // namespace exios

#endif // EXIOS_IO_HPP_INCLUDED
