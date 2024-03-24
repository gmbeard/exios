#include "exios/contracts.hpp"
#include <cstdio>
#include <exception>

namespace exios
{
auto contract_check_failed(char const* msg, std::source_location loc) -> void
{
    std::fprintf(stderr,
                 "Contract check failed: %s - %s:%u\n",
                 msg,
                 loc.file_name(),
                 loc.line());
    std::terminate();
}
} // namespace exios
