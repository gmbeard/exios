#include "exios/exios.hpp"
#include "exios/intrusive_list.hpp"
#include "testing.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <vector>

struct TestListItem : exios::ListItemBase
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

template <typename ForwardIt, typename T, typename F>
ForwardIt
lower_bound_n(ForwardIt first,
              typename std::iterator_traits<ForwardIt>::difference_type n,
              const T& value,
              F comp)
{
    ForwardIt it = first;
    typename std::iterator_traits<ForwardIt>::difference_type count, step;
    count = n; // std::distance(first, last);

    while (count > 0) {
        it = first;
        step = count / 2;
        std::advance(it, step);

        if (comp(*it, value)) {
            first = ++it;
            count -= step + 1;
        }
        else
            count = step;
    }

    return first;
}

template <typename I, typename F>
auto check_loop(I first, I last, std::size_t expected_count, F f) -> bool
{
    std::size_t i = 0;
    while (first != last) {
        if (i++ == expected_count)
            return false;
        f(*first++);
    }

    return i == expected_count;
}

template <typename I>
auto check_loop(I first, I last, std::size_t expected_count) -> bool
{
    return check_loop(first, last, expected_count, [](auto const&) {});
}

auto should_splice_items_from_other_list() -> void
{
    std::array<TestListItem, 5> items { TestListItem(1),
                                        TestListItem(2),
                                        TestListItem(3),
                                        TestListItem(4),
                                        TestListItem(5) };

    exios::IntrusiveList<TestListItem> list, tmp;

    std::transform(items.begin(),
                   items.end(),
                   std::back_inserter(list),
                   [](auto& val) { return std::addressof(val); });

    EXPECT(!list.empty());

    static_cast<void>(tmp.splice(tmp.end(), list));

    EXPECT(list.empty());
    EXPECT(!tmp.empty());

    EXPECT(check_loop(tmp.begin(), tmp.end(), items.size()));
}

auto should_splice_single_item_in_same_list() -> void
{
    std::array items { TestListItem(1) };

    exios::IntrusiveList<TestListItem> list;

    std::transform(items.begin(),
                   items.end(),
                   std::back_inserter(list),
                   [](auto& val) { return std::addressof(val); });

    EXPECT(!list.empty());

    static_cast<void>(
        list.splice(list.end(), list.begin(), std::next(list.begin(), 1)));
}

auto should_splice_items_in_same_list() -> void
{
    std::array<TestListItem, 5> items { TestListItem(1),
                                        TestListItem(2),
                                        TestListItem(3),
                                        TestListItem(4),
                                        TestListItem(5) };

    exios::IntrusiveList<TestListItem> list;

    std::transform(items.begin(),
                   items.end(),
                   std::back_inserter(list),
                   [](auto& val) { return std::addressof(val); });

    EXPECT(!list.empty());

    std::cerr << "BEFORE:\n";
    for (auto const& i : list) {
        std::cerr << i.value << '\n';
    }

    [[maybe_unused]] auto splice_point =
        list.splice(std::next(list.end(), 0),
                    list,
                    list.begin(),
                    std::next(list.begin(), 1));

    std::cerr << "AFTER:\n";
    EXPECT(
        check_loop(list.begin(), list.end(), items.size(), [&](auto const& i) {
            std::cerr << i.value << '\n';
        }));

    EXPECT(check_loop(splice_point, list.end(), 1));

    auto it = splice_point;
    while (it != list.end()) {
        it = list.erase(it);
    }

    EXPECT(!list.empty());
    EXPECT(check_loop(list.begin(), list.end(), 4));
}

auto should_splice_whole_list_onto_itself() -> void
{
    std::array items { TestListItem(1) };

    exios::IntrusiveList<TestListItem> list;

    std::transform(items.begin(),
                   items.end(),
                   std::back_inserter(list),
                   [](auto& val) { return std::addressof(val); });

    auto splice_point = list.splice(list.end(), list, list.begin(), list.end());
    EXPECT(splice_point == list.begin());
}

auto should_sort_insert_lots_of_elements() -> void
{
    constexpr std::size_t kNumElements = 500'000;
    std::vector<TestListItem> storage(kNumElements);
    exios::IntrusiveList<TestListItem> list;

    for (std::size_t n = 0; n < kNumElements; ++n) {
        auto pos = list.end();

        if (!list.empty() && static_cast<int>(n) <= list.front().value)
            pos = list.begin();
        else if (!list.empty() && static_cast<int>(n) >= list.back().value)
            pos = list.end();
        else if (!list.empty() && static_cast<int>(n) > list.front().value)
            pos = lower_bound_n(
                list.begin(), n, n, [](auto const& item, auto val) {
                    return item.value < static_cast<int>(val);
                });

        storage[n].value = n;
        static_cast<void>(list.insert(&storage[n], pos));
    }
}

auto should_sort_insert_from_random_elements() -> void
{
    constexpr std::size_t kNumElements = 1'000;
    std::vector<TestListItem> storage(kNumElements);
    exios::IntrusiveList<TestListItem> list;

    std::random_device device;
    std::mt19937 mt { device() };

    for (std::size_t n = 0; n < kNumElements; ++n) {
        auto pos = list.end();

        int random_val = mt() % std::numeric_limits<int>::max();

        if (!list.empty() && random_val <= list.front().value)
            pos = list.begin();
        else if (!list.empty() && random_val >= list.back().value)
            pos = list.end();
        else if (!list.empty() && random_val > list.front().value)
            pos = lower_bound_n(
                list.begin(), n, random_val, [](auto const& item, auto val) {
                    return item.value < val;
                });

        storage[n].value = random_val;
        static_cast<void>(list.insert(&storage[n], pos));
    }
}

auto main() -> int
{
    return testing::run({ TEST(should_splice_items_in_same_list),
                          TEST(should_splice_items_from_other_list),
                          TEST(should_splice_single_item_in_same_list),
                          TEST(should_splice_whole_list_onto_itself),
                          TEST(should_sort_insert_lots_of_elements),
                          TEST(should_sort_insert_from_random_elements) });
}
