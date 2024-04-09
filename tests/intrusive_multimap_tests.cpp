#include "exios/exios.hpp"
#include "testing.hpp"
#include <iostream>
#include <random>

struct TestListItem : exios::MultiMapItemBase<int>
{
    TestListItem()
        : value { 0 }
    {
    }

    explicit TestListItem(int val) noexcept
        : value { val }
    {
    }

    int value;
};

auto should_insert_elements() -> void
{
    std::vector<TestListItem> storage { TestListItem { 6 }, TestListItem { 3 },
                                        TestListItem { 1 }, TestListItem { 4 },
                                        TestListItem { 2 }, TestListItem { 3 },
                                        TestListItem { 5 } };

    exios::IntrusiveMultiMap<int, TestListItem> map;

    for (auto& element : storage) {
        map.insert(element.value, &element);
    }

    auto pos = map.find(5);
    EXPECT(pos != map.end());
    EXPECT(pos->value == 5);

    auto n = storage.size();
    for (auto& element : map) {
        if (n-- == 0)
            break;
        std::cerr << element.value << ", ";
    }

    std::cerr << '\n';
    EXPECT(false);
}

auto should_sort_insert_from_random_elements() -> void
{
    constexpr std::size_t kNumElements = 500'000;
    std::vector<TestListItem> storage(kNumElements);
    exios::IntrusiveMultiMap<int, TestListItem> map;

    std::random_device device;
    std::mt19937 mt { device() };

    int key_to_search = 0;

    for (std::size_t n = 0; n < kNumElements; ++n) {

        auto const key = mt() % std::numeric_limits<int>::max();
        if (n == 0)
            key_to_search = key;

        storage[n].value = key;
        map.insert(key, &storage[n]);
    }

    EXPECT(!map.empty());
    EXPECT(map.find(key_to_search) != map.end());
}

auto main() -> int
{
    return testing::run({ TEST(should_insert_elements)/*,
                          TEST(should_sort_insert_from_random_elements) */});
}
