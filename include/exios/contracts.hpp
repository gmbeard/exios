#ifndef EXIOS_UTILS_CONTRACTS_HPP_INCLUDED
#define EXIOS_UTILS_CONTRACTS_HPP_INCLUDED

#include <source_location>

#define EXIOS_STRINGIFY_IMPL(s) #s
#define EXIOS_STRINGIFY(s) EXIOS_STRINGIFY_IMPL(s)

#define EXIOS_EXPECT(cond)                                                     \
    do {                                                                       \
        if (!(cond))                                                           \
            ::exios::contract_check_failed(EXIOS_STRINGIFY(cond));             \
    }                                                                          \
    while (0)

namespace exios
{

[[noreturn]] auto contract_check_failed(
    char const* /*msg*/,
    std::source_location /*loc*/ = std::source_location::current()) -> void;

} // namespace exios

#endif // EXIOS_UTILS_CONTRACTS_HPP_INCLUDED
