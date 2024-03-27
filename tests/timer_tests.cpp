#include "exios/exios.hpp"
#include "testing.hpp"
#include <cinttypes>
#include <iostream>

auto timer_should_expire() -> void
{
    exios::ContextThread thread;
    exios::Timer timer { thread };

    bool expired = false;

    timer.wait_for_expiry_after(
        std::chrono::milliseconds(1),
        [&](exios::Result<std::uint64_t, std::error_code> result) {
            EXPECT(!result.is_error_value());
            std::cerr << result.value() << '\n';
            EXPECT(result.value() == 1);
            expired = true;
        });

    EXPECT(!expired);
    static_cast<void>(thread.run());
    EXPECT(expired);
}

auto main() -> int { return testing::run({ TEST(timer_should_expire) }); }
