#ifndef EXIOS_BUFFER_VIEW_HPP_INCLUDED
#define EXIOS_BUFFER_VIEW_HPP_INCLUDED

#include <cstddef>

namespace exios
{
struct BufferView
{
    void* data { nullptr };
    std::size_t size { 0 };
};

struct ConstBufferView
{
    void const* data { nullptr };
    std::size_t size { 0 };
};

} // namespace exios

#endif // EXIOS_BUFFER_VIEW_HPP_INCLUDED
