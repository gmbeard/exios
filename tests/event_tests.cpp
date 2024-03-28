#include "exios/exios.hpp"
#include "testing.hpp"

auto should_trigger_event() -> void
{
    exios::ContextThread thread;
    exios::Event event { thread };

    bool event_delivered = false;
    bool event_posted = false;

    event.wait_for_event([&](auto result) {
        EXIOS_EXPECT(!result.is_error_value());
        event_delivered = true;
    });

    event.trigger([&](auto result) {
        EXIOS_EXPECT(!result.is_error_value());
        event_posted = true;
    });

    static_cast<void>(thread.run());

    EXPECT(event_posted);
    EXPECT(event_delivered);
}

auto main() -> int { return testing::run({ TEST(should_trigger_event) }); }
