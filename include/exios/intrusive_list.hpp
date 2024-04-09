#ifndef EXIOS_UTILS_INTRUSIVE_LIST_HPP_INCLUDED
#define EXIOS_UTILS_INTRUSIVE_LIST_HPP_INCLUDED

#include "exios/contracts.hpp"
#include <cstddef>
#include <iterator>
#include <type_traits>

namespace exios
{

struct ListItemBase
{
    ListItemBase* prev { nullptr };
    ListItemBase* next { nullptr };
};

template <typename T>
concept ListItem = std::is_convertible_v<T&, ListItemBase const&>;

template <ListItem T>
struct IntrusiveList
{
    IntrusiveList() noexcept { sentinel_.next = sentinel_.prev = &sentinel_; }

    /* Not copyable; We don't know how to allocate
     * or copy items...
     */
    IntrusiveList(IntrusiveList const&) = delete;
    auto operator=(IntrusiveList const&) -> IntrusiveList& = delete;

    IntrusiveList(IntrusiveList&& other) noexcept
    {
        if (other.empty()) {
            sentinel_.next = sentinel_.prev = &sentinel_;
        }
        else {
            other.sentinel_.next->prev = &sentinel_;
            other.sentinel_.prev->next = &sentinel_;

            other.sentinel_.next = other.sentinel_.prev = &other.sentinel_;
        }
    }

    auto operator=(IntrusiveList&& other) noexcept -> IntrusiveList&
    {
        if (this == &other)
            return *this;

        using std::swap;
        auto tmp { std::move(other) };
        swap(*this, tmp);
        return *this;
    }

    template <bool IsConst>
    struct Iterator
    {
        using difference_type = std::ptrdiff_t;
        using value_type = std::conditional_t<IsConst,
                                              std::remove_const_t<T> const,
                                              std::remove_const_t<T>>;
        using pointer = value_type*;
        using reference = value_type&;
        using const_reference = std::remove_const_t<value_type> const&;
        using iterator_category = std::bidirectional_iterator_tag;

        using Element =
            std::conditional_t<IsConst, ListItemBase const, ListItemBase>;

        Element* current;

        Iterator(Element* item) noexcept
            : current { item }
        {
        }

        Iterator(Iterator<false> const& other) noexcept
        requires(IsConst)
            : current { other.current }
        {
        }

        Iterator(Iterator const& other) noexcept
            : current { other.current }
        {
        }

        auto operator=(Iterator const& other) noexcept -> Iterator&
        {
            current = other.current;
            return *this;
        }

        auto operator=(Iterator<false> const& other) noexcept -> Iterator&
        requires(IsConst)
        {
            current = other.current;
            return *this;
        }

        [[nodiscard]] auto operator->() const noexcept -> pointer
        {
            return static_cast<value_type*>(current);
        }

        [[nodiscard]] auto operator*() noexcept -> reference
        requires(!IsConst)
        {
            return *static_cast<value_type*>(current);
        }

        [[nodiscard]] auto operator*() const noexcept -> const_reference
        {
            return *static_cast<std::remove_const_t<value_type> const*>(
                current);
        }

        auto operator++(int) noexcept -> Iterator
        {
            auto tmp { *this };
            operator++();
            return tmp;
        }

        auto operator++() noexcept -> Iterator&
        {
            current = current->next;
            return *this;
        }

        auto operator--(int) noexcept -> Iterator
        {
            auto tmp { *this };
            operator--();
            return tmp;
        }

        auto operator--() noexcept -> Iterator&
        {
            current = current->prev;
            return *this;
        }

        [[nodiscard]] auto operator==(Iterator const& other) const noexcept
            -> bool
        {
            return current == other.current;
        }

        [[nodiscard]] auto operator!=(Iterator const& other) const noexcept
            -> bool
        {
            return current != other.current;
        }
    };

    using iterator = Iterator<false>;
    using const_iterator = Iterator<true>;

    [[nodiscard]] auto empty() const noexcept -> bool
    {
        return begin() == end();
    }

    [[nodiscard]] auto erase(iterator pos) noexcept -> iterator
    {
        EXIOS_EXPECT(pos != end());
        EXIOS_EXPECT(!empty());
        auto* item = pos.current;
        auto next_item = std::next(pos);

        auto* prv = item->prev;
        auto* nxt = item->next;

        prv->next = nxt;
        nxt->prev = prv;

        return next_item;
    }

    [[nodiscard]] auto front() noexcept -> typename iterator::reference
    {
        // cppcheck-suppress [nullPointerRedundantCheck]
        EXIOS_EXPECT(!empty());
        return *begin();
    }

    [[nodiscard]] auto front() const noexcept ->
        typename const_iterator::reference
    {
        EXIOS_EXPECT(!empty());
        return *begin();
    }

    [[nodiscard]] auto back() noexcept -> typename iterator::reference
    {
        EXIOS_EXPECT(!empty());
        return *std::next(end(), -1);
    }

    [[nodiscard]] auto back() const noexcept ->
        typename const_iterator::reference
    {
        EXIOS_EXPECT(!empty());
        return *std::next(end(), -1);
    }

    auto push_front(T* item) noexcept -> void
    {
        EXIOS_EXPECT(item);
        sentinel_.next->prev = item;
        // cppcheck-suppress [nullPointerRedundantCheck]
        item->prev = &sentinel_;
        // cppcheck-suppress [nullPointerRedundantCheck]
        item->next = sentinel_.next;
        sentinel_.next = item;
    }

    auto push_back(T* item) noexcept -> void
    {
        EXIOS_EXPECT(item);
        sentinel_.prev->next = item;
        // cppcheck-suppress [nullPointerRedundantCheck]
        item->next = &sentinel_;
        // cppcheck-suppress [nullPointerRedundantCheck]
        item->prev = sentinel_.prev;
        sentinel_.prev = item;
    }

    auto pop_back() noexcept -> void
    {
        EXIOS_EXPECT(!empty());

        auto* item_to_remove = sentinel_.prev;

        sentinel_.prev = item_to_remove->prev;
        sentinel_.prev->next = &sentinel_;
    }

    auto pop_front() noexcept -> void
    {
        EXIOS_EXPECT(!empty());

        auto* item_to_remove = sentinel_.next;
        sentinel_.next = item_to_remove->next;
        sentinel_.next->prev = &sentinel_;
    }

    [[nodiscard]] auto insert(T* item, iterator before) noexcept -> iterator
    {
        EXIOS_EXPECT(item);

        auto* before_item = before.current;
        before_item->prev->next = item;
        // cppcheck-suppress [nullPointerRedundantCheck]
        item->prev = before_item->prev;
        // cppcheck-suppress [nullPointerRedundantCheck]
        item->next = before_item;
        before_item->prev = item;
        return before;
    }

    [[nodiscard]] auto splice(iterator before,
                              IntrusiveList& other,
                              iterator first,
                              iterator last) noexcept -> iterator
    {
        EXIOS_EXPECT(!(&other == this && before == first));

        /* No-op if we're trying to splice the whole list
         * onto itself. Just return `first`...
         */
        if (&other == this && first == other.begin() && last == other.end())
            return first;

        auto it = first;
        while (it != last) {
            auto* val = &*it;
            it = other.erase(it);
            before = insert(val, before);
        }

        return first;
    }

    [[nodiscard]] auto
    splice(iterator before, iterator first, iterator last) noexcept -> iterator
    {
        return splice(before, *this, first, last);
    }

    [[nodiscard]] auto splice(iterator before, IntrusiveList& other) noexcept
        -> iterator
    {
        return splice(before, other, other.begin(), other.end());
    }

    [[nodiscard]] auto begin() noexcept -> iterator
    {
        return iterator { sentinel_.next };
    }
    [[nodiscard]] auto begin() const noexcept -> const_iterator
    {
        return const_iterator { sentinel_.next };
    }

    [[nodiscard]] auto end() noexcept -> iterator
    {
        return iterator { &sentinel_ };
    }
    [[nodiscard]] auto end() const noexcept -> const_iterator
    {
        return const_iterator { &sentinel_ };
    }

private:
    ListItemBase sentinel_;
};

template <typename T, typename F>
auto drain_list(exios::IntrusiveList<T>& list,
                F&& op) noexcept(noexcept(op(list.front()))) -> void
{
    while (!list.empty()) {
        auto item = &list.front();
        list.pop_front();
        op(std::move(*item));
    }
}

} // namespace exios
#endif // EXIOS_UTILS_INTRUSIVE_LIST_HPP_INCLUDED
