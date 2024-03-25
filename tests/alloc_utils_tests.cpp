#include "exios/exios.hpp"
#include "testing.hpp"
#include "tracking_allocator.hpp"
#include <iostream>

auto should_use_custom_allocator_with_timer_io() -> void
{
    bool used_custom_allocator = false;
    std::intptr_t allocations = 0;
    std::intptr_t max_allocations = 0;

    auto alloc_callback = [&](auto const& allocating, auto const&) {
        if (allocating) {
            allocations += 1;
            max_allocations = std::max(max_allocations, allocations);
        }
        else {
            allocations -= 1;
        }
        used_custom_allocator = true;
    };

    auto cb = [&](exios::IoResult&&) { EXPECT(allocations == 0); };

    exios::ContextThread thread;
    exios::Timer timer { thread };
    timer.wait_for_expiry_after(
        std::chrono::milliseconds(1),
        exios::use_allocator(std::move(cb),
                             tracking_allocator<void>(alloc_callback)));

    static_cast<void>(thread.run());
    EXPECT(used_custom_allocator);
    std::cerr << max_allocations << '\n';
    EXPECT(max_allocations > 0);
}

auto main() -> int
{
    return testing::run({ TEST(should_use_custom_allocator_with_timer_io) });
}
