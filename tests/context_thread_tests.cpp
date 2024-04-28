#include "exios/exios.hpp"
#include "testing.hpp"
#include <atomic>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

auto should_only_throw_on_run() -> void
{
    exios::ContextThread thread;

    thread.post(
        [&] { throw std::runtime_error { "should_only_throw_on_run" }; });

    EXPECT_THROWS(thread.run());
}

auto should_be_exception_safe() -> void
{
    exios::ContextThread thread;

    std::size_t completed = 0;

    thread.post([&] {
        completed += 1;
        throw std::runtime_error { "should_be_exception_safe 1" };
    });

    thread.post([&] {
        completed += 1;
        thread.post([&] {
            completed += 1;
            throw std::runtime_error { "should_be_exception_safe 4" };
        });
        throw std::runtime_error { "should_be_exception_safe 2" };
    });

    thread.post([&] {
        completed += 1;
        throw std::runtime_error { "should_be_exception_safe 3" };
    });

    do {
        try {
            static_cast<void>(thread.run());
            break;
        }
        catch (std::exception const& e) {
            std::cerr << "Error handled: " << e.what() << '\n';
        }
    }
    while (true);

    std::cerr << completed << '\n';
    EXPECT(completed == 4);
}

auto should_be_exception_safe_multi_threaded() -> void
{
    exios::ContextThread thread;
    exios::Work<exios::ContextThread> work { thread };

    std::atomic_size_t completed = 0;

    std::vector<std::thread> threads(std::thread::hardware_concurrency());
    for (auto& t : threads) {
        t = std::thread { [&] {
            do {
                try {
                    static_cast<void>(thread.run());
                    break;
                }
                catch (std::exception const& e) {
                    std::cerr << "Error handled: " << e.what() << '\n';
                }
            }
            while (true);
        } };
    }

    thread.post([&] {
        completed += 1;
        throw std::runtime_error {
            "should_be_exception_safe_multi_threaded 1"
        };
    });

    thread.post([&] {
        completed += 1;
        thread.post([&] {
            completed += 1;
            throw std::runtime_error {
                "should_be_exception_safe_multi_threaded 4"
            };
        });
        throw std::runtime_error {
            "should_be_exception_safe_multi_threaded 2"
        };
    });

    thread.post([&] {
        completed += 1;
        throw std::runtime_error {
            "should_be_exception_safe_multi_threaded 3"
        };
    });

    thread.post([&] { work.reset(); });

    for (auto& t : threads)
        t.join();

    std::cerr << completed << '\n';
    EXPECT(completed == 4);
}

auto main() -> int
{
    return testing::run({ TEST(should_be_exception_safe),
                          TEST(should_only_throw_on_run),
                          TEST(should_be_exception_safe_multi_threaded) });
}
