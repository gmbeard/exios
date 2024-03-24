#include "exios/async_operation.hpp"

namespace exios
{

auto discard(AnyAsyncOperation&& op) noexcept -> void
{
    std::move(op).discard();
}

auto dispatch(AnyAsyncOperation&& op) noexcept -> void
{
    std::move(op).dispatch();
}

} // namespace exios
