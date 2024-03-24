#include "exios/io.hpp"

namespace exios
{
auto perform_read(int, BufferView) noexcept -> IoResult
{
    return std::make_error_code(std::errc::operation_canceled);
}

auto perform_write(int, ConstBufferView) noexcept -> IoResult
{
    return std::make_error_code(std::errc::operation_canceled);
}

} // namespace exios
