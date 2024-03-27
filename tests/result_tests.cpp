#include "exios/exios.hpp"
#include "testing.hpp"
#include <system_error>

auto use_result(exios::IoResult result) -> void
{
    EXPECT(result.is_error_value());
}

auto should_retain_result_state_on_implicit_conversion() -> void
{
    exios::IoResult val = exios::result_error(
        std::make_error_code(std::errc::operation_canceled));
    use_result(std::move(val));
}

auto main() -> int
{
    return testing::run(
        { TEST(should_retain_result_state_on_implicit_conversion) });
}
