#include "./tracking_allocator.hpp"
#include "exios/exios.hpp"
#include "exios/intrusive_list.hpp"
#include "testing.hpp"
#include <algorithm>
#include <cstdint>
#include <memory>
#include <numeric>

auto should_make_async_io_operation() -> void
{
    exios::ContextThread thread;

    auto cb = [](exios::IoResult) {};

    auto* op =
        exios::make_async_io_operation(exios::write_operation,
                                       exios::wrap_work(std::move(cb), thread),
                                       std::allocator<void> {},
                                       thread,
                                       0,
                                       exios::ConstBufferView {});
    EXIOS_SCOPE_GUARD([&] { discard(std::move(*op)); });
}

auto should_invoke_async_io_operation() -> void
{
    exios::ContextThread thread;

    bool invoked = false;
    auto cb = [&](exios::IoResult) { invoked = true; };

    {
        auto* op = exios::make_async_io_operation(
            exios::write_operation,
            exios::wrap_work(std::move(cb), thread),
            std::allocator<void> {},
            thread,
            0,
            exios::ConstBufferView {});
        EXIOS_SCOPE_GUARD([&] { dispatch(std::move(*op)); });

        static_cast<void>(op->perform_io());

        EXPECT(!invoked);
    }

    EXPECT(invoked);
}

auto should_deallocate_before_invocation() -> void
{
    exios::ContextThread thread;

    std::int32_t allocations = 0;

    [[maybe_unused]] auto alloc_cb = [&](auto allocating, auto num) {
        if (allocating) {
            allocations += num;
        }
        else {
            allocations -= num;
        }
    };

    bool callback_invoked = false;
    auto cb = [&](exios::IoResult) {
        EXPECT(allocations == 0);
        callback_invoked = true;
    };

    auto* op =
        exios::make_async_io_operation(exios::write_operation,
                                       exios::wrap_work(std::move(cb), thread),
                                       tracking_allocator<void>(alloc_cb),
                                       exios::Context { thread },
                                       0,
                                       exios::ConstBufferView {});
    thread.io_scheduler().schedule(op);

    EXPECT(!callback_invoked);
    auto const num = thread.run();
    EXPECT(num == 1);
    EXPECT(callback_invoked);
}

auto should_behave_in_intrusive_list() -> void
{
    exios::ContextThread thread;

    exios::IntrusiveList<exios::AsyncIoOperation> list;
    auto cb = [&](exios::IoResult) {};

    auto* op =
        exios::make_async_io_operation(exios::write_operation,
                                       exios::wrap_work(std::move(cb), thread),
                                       std::allocator<void> {},
                                       exios::Context { thread },
                                       0,
                                       exios::ConstBufferView {});

    static_cast<void>(list.insert(
        op,
        std::lower_bound(
            list.begin(), list.end(), *op, [](auto const& a, auto const& b) {
                return a.get_fd() < b.get_fd();
            })));

    auto count = std::accumulate(
        list.begin(), list.end(), 0, [](auto const& acc, auto const&) {
            return acc + 1;
        });

    EXPECT(count == 1);

    auto next = list.erase(list.begin());
    count = std::accumulate(
        list.begin(), list.end(), 0, [](auto const& acc, auto const&) {
            return acc + 1;
        });

    EXPECT(count == 0);
    EXPECT(next == list.end());

    discard(std::move(*op));
}

auto main() -> int
{
    return testing::run({ TEST(should_make_async_io_operation),
                          TEST(should_invoke_async_io_operation),
                          TEST(should_deallocate_before_invocation),
                          TEST(should_behave_in_intrusive_list) });
}
