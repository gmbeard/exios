#include "exios/io.hpp"
#include <cstddef>
#include <errno.h>
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

auto perform_timer_read(int fd) noexcept -> TimerIoResult
{
    std::uint64_t val;
    auto const result = ::read(fd, &val, sizeof(val));
    if (result < 0)
        return result_error(std::error_code { errno, std::system_category() });

    return result_ok(val);
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

auto TimerExpiry::io(int fd) noexcept -> bool
{
    EXIOS_EXPECT(!result_);
    auto r = perform_timer_read(fd);
    if (r.is_error_value() && r.error() == std::errc::operation_would_block)
        return false;

    result_.emplace(r);
    return true;
}

} // namespace exios
