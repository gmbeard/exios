#include "exios/exios.hpp"
#include "testing.hpp"
#include <chrono>
#include <iostream>

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

auto should_cancel_all_previous_timer_waits() -> void
{
    exios::ContextThread thread;
    exios::Timer timer { thread };

    auto constexpr kMaxRequests = 10;
    std::uint32_t nrequests = kMaxRequests;

    std::uint32_t expired_count = 0;
    std::uint32_t cancelled_count = 0;

    for (auto i = 0ul; i < nrequests; ++i) {
        timer.wait_for_expiry_after(
            std::chrono::milliseconds(50), [&](auto const& result) {
                EXPECT(result ||
                       result.error() == std::errc::operation_canceled);

                if (result)
                    expired_count += 1;
                else
                    cancelled_count += 1;
            });
    }

    static_cast<void>(thread.run());

    EXPECT(expired_count == 1);
    std::cerr << "Cancelled: " << cancelled_count
              << "\nExpired: " << expired_count << '\n';
    EXPECT((kMaxRequests - expired_count) == cancelled_count);
    EXPECT((expired_count + cancelled_count) == kMaxRequests);
}

auto should_cancel_other_timers() -> void
{

    exios::ContextThread thread;
    exios::Timer timer { thread };

    constexpr int kMaxItems = 3;
    int num_items = kMaxItems;
    for (auto i = 0; i < num_items; ++i) {
        timer.wait_for_expiry_after(std::chrono::milliseconds(10),
                                    [&](auto const&) {
                                        if (num_items-- == kMaxItems) {
                                            timer.cancel();
                                        }
                                    });
    }

    static_cast<void>(thread.run());
}

auto main() -> int
{
    return testing::run({ TEST(should_cancel_timer),
                          TEST(should_cancel_other_timers),
                          TEST(should_cancel_all_previous_timer_waits) });
}
