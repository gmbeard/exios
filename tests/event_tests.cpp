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

auto should_operate_in_semaphore_mode() -> void
{
    exios::ContextThread thread;
    exios::Event event { thread, exios::semaphone_mode };

    std::size_t events_delivered = 0;
    bool event_posted = false;

    event.wait_for_event([&](auto result) {
        EXIOS_EXPECT(!result.is_error_value());
        events_delivered += 1;
        event.wait_for_event([&](auto next_result) {
            EXIOS_EXPECT(!next_result.is_error_value());
            events_delivered += 1;
        });
    });

    event.trigger_with_value(2, [&](auto result) {
        EXIOS_EXPECT(!result.is_error_value());
        event_posted = true;
    });

    static_cast<void>(thread.run());

    EXPECT(event_posted);
    EXPECT(events_delivered == 2);
}

auto main() -> int
{
    return testing::run(
        { TEST(should_trigger_event), TEST(should_operate_in_semaphore_mode) });
}
