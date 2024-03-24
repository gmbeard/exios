#ifndef EXIOS_IO_HPP_INCLUDED
#define EXIOS_IO_HPP_INCLUDED

#include "exios/buffer_view.hpp"
#include "exios/result.hpp"
#include <system_error>

namespace exios
{
using IoResult = Result<std::size_t, std::error_code>;

auto perform_read(int fd, BufferView buffer) noexcept -> IoResult;
auto perform_write(int fd, ConstBufferView buffer) noexcept -> IoResult;
} // namespace exios

#endif // EXIOS_IO_HPP_INCLUDED
