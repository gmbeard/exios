\mainpage Exios

Exios is a C++ library for building event-based applications.

A simple example that waits for a 10 second timer to expire then exits.

```cpp
#include <exios/exios.hpp>
#include <chrono>

auto main() -> int
{
    exios::ContextThread thread;
    exios::Timer timer { thread };

    timer.wait_for_expiry_after(
        std::chrono::seconds(10),
        [](exios::TimerExpiryOrEvent const& result) {
            if (result)
                std::cout << "Timer expired!\n";
        });

    auto const num_processed = thread.run();
    std::cout << "Processed " << num_processed << " events\n";
}

```
