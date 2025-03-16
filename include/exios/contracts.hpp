#ifndef EXIOS_UTILS_CONTRACTS_HPP_INCLUDED
#define EXIOS_UTILS_CONTRACTS_HPP_INCLUDED

#define EXIOS_STRINGIFY_IMPL(s) #s
#define EXIOS_STRINGIFY(s) EXIOS_STRINGIFY_IMPL(s)

#define EXIOS_EXPECT(cond)                                                     \
    do {                                                                       \
        if (!(cond)) {                                                         \
            ::exios::contract_check_failed(                                    \
                EXIOS_STRINGIFY(cond), __FILE__, __LINE__);                    \
            /* Seems this is necessary to inform cppcheck that the check isn't \
             * redundant. It doesn't seem to understand the [[noreturn]]       \
             * attribute of contract_check_failed...                           \
             */                                                                \
            __builtin_unreachable();                                           \
        }                                                                      \
    }                                                                          \
    while (0)

namespace exios
{

[[noreturn]] auto contract_check_failed(char const* /*msg*/,
                                        char const* /* file */,
                                        unsigned int /* line */) -> void;

} // namespace exios

#endif // EXIOS_UTILS_CONTRACTS_HPP_INCLUDED
