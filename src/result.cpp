#include "exios/result.hpp"

namespace exios
{
auto result_ok() noexcept -> detail::VoidResult
{
    return detail::VoidResult {};
}
} // namespace exios
