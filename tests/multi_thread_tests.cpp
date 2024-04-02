#include "exios/exios.hpp"
#include "testing.hpp"
#include <atomic>
#include <iostream>
#include <sys/sysinfo.h>
#include <thread>
#include <vector>

auto should_expire_timer_on_background_thread() -> void
{
    auto const num_threads_to_use = std::max(get_nprocs(), 4);

    std::cerr << "Testing with " << num_threads_to_use << " threads\n";

    exios::ContextThread thread;
    exios::Timer timer { thread };
    exios::Work work { thread };

    std::vector<std::thread> threads(num_threads_to_use);
    for (auto& t : threads) {
        t = std::thread { [&] { static_cast<void>(thread.run()); } };
    }

    std::thread::id thread_id = std::this_thread::get_id();
    auto constexpr kMaxRequests = 10;

    std::atomic_uint32_t pending_requests = kMaxRequests;
    std::atomic_uint32_t expired_count = 0;
    std::atomic_uint32_t cancelled_count = 0;

    for (auto i = 0ul; i < kMaxRequests; ++i) {
        timer.wait_for_expiry_after(
            std::chrono::milliseconds(50), [&](auto const& result) {
                EXPECT(result ||
                       result.error() == std::errc::operation_canceled);

                if (result)
                    expired_count += 1;
                else
                    cancelled_count += 1;

                if (pending_requests.fetch_sub(1) == 1) {
                    thread_id = std::this_thread::get_id();
                    work.reset();
                }
            });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT((expired_count + cancelled_count) == kMaxRequests);
    EXPECT(thread_id != std::this_thread::get_id());
    EXPECT(expired_count == 1);
    std::cerr << "Cancelled: " << cancelled_count
              << "\nExpired: " << expired_count << '\n';
    EXPECT((kMaxRequests - expired_count) == cancelled_count);
}

auto main() -> int
{
    return testing::run({ TEST(should_expire_timer_on_background_thread) });
}
