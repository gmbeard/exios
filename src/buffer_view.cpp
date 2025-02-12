#include "exios/buffer_view.hpp"

namespace exios
{
auto const_buffer_view(std::string_view val) noexcept -> ConstBufferView
{
    return { val.data(), val.size() };
}
} // namespace exios
