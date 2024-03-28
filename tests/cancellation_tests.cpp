#include "exios/exios.hpp"
#include "testing.hpp"
#include <chrono>

auto should_cancel_timer() -> void
{
    exios::ContextThread thread;
    exios::Timer timer { thread };

    bool timer_expired = false;

    timer.wait_for_expiry_after(
        std::chrono::milliseconds(5000), [&](auto&& result) {
            timer_expired = result.is_result_value() ||
                            result.error() != std::errc::operation_canceled;
        });

    timer.cancel();

    static_cast<void>(thread.run());

    EXPECT(!timer_expired);
}

auto main() -> int { return testing::run({ TEST(should_cancel_timer) }); }
