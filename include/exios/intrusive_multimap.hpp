#ifndef EXIOS_INTRUSIVE_MULTIMAP_HPP_INCLUDED
#define EXIOS_INTRUSIVE_MULTIMAP_HPP_INCLUDED

#include "exios/contracts.hpp"
#include "exios/intrusive_list.hpp"
#include <algorithm>

namespace exios
{

template <typename Key>
struct MultiMapItemBase : ListItemBase
{
    using key_type = Key;

    MultiMapItemBase* left { nullptr };
    MultiMapItemBase* right { nullptr };
    Key key {};
};

template <typename T, typename K>
concept MultiMapItem =
    std::is_convertible_v<T&, MultiMapItemBase<K> const&> && ListItem<T>;

template <typename Key, MultiMapItem<Key> T>
struct IntrusiveMultiMap
{
    using value_type = T;
    using key_type = typename T::key_type;
    using reference = value_type&;
    using const_reference = value_type const&;
    using base_node = MultiMapItemBase<key_type>;
    using base_pointer = MultiMapItemBase<key_type>*;
    using pointer = value_type*;
    using const_pointer = value_type const*;
    using iterator = IntrusiveList<T>::iterator;
    using const_iterator = IntrusiveList<T>::const_iterator;

    auto insert(key_type const& key, pointer element) noexcept -> void
    {
        iterator pos = end();
        auto& node = find_insert_position(key, pos);
        element->key = key;
        pointer item = element;
        if (node == nullptr) {
            node = element;
            item = static_cast<pointer>(node);
        }

        static_cast<void>(elements_.insert(item, pos));
    }

    auto remove(key_type const& /*key*/,
                IntrusiveList<value_type>& /*removed_items*/) noexcept -> void
    {
        EXIOS_EXPECT(!elements_.empty());
        //
    }

    auto find(key_type const& key) noexcept -> iterator
    {
        using base = MultiMapItemBase<key_type>;

        if (!root_.left)
            return elements_.end();

        base* branch = root_.left;
        base* leaf = root_.left;

        while (leaf && leaf->key != key) {
            branch = leaf;
            if (key < branch->key)
                leaf = branch->left;
            else
                leaf = branch->right;
        }

        if (!leaf)
            return elements_.end();

        return iterator { reinterpret_cast<ListItemBase*>(leaf) };
    }

    auto find(key_type const& key) const noexcept -> const_iterator
    {
        using base = MultiMapItemBase<key_type> const;

        if (!root_.left)
            return elements_.end();

        base* branch = root_.left;
        base* leaf = root_.left;

        while (leaf && leaf->key != key) {
            branch = leaf;
            if (key < branch->key)
                leaf = branch->left;
            else
                leaf = branch->right;
        }

        if (!leaf)
            return elements_.end();

        return const_iterator { reinterpret_cast<ListItemBase const*>(leaf) };
    }

    auto empty() const noexcept -> bool { return elements_.empty(); }

    auto begin() noexcept -> iterator { return elements_.begin(); }

    auto begin() const noexcept -> const_iterator { return elements_.begin(); }

    auto end() noexcept -> iterator { return elements_.end(); }

    auto end() const noexcept -> const_iterator { return elements_.end(); }

private:
    auto find_insert_position(key_type const& key, iterator& pos) noexcept
        -> base_pointer&
    {
        if (!root_.left) {
            pos = end();
            return root_.left;
        }

        base_pointer node = root_.left;
        while (true) {
            if (key < node->key) {
                if (node->left == nullptr) {
                    pos = iterator { node };
                    return node->left;
                }
                else {
                    if (key == node->left->key) {
                        pos = iterator { node->left };
                        return node->left;
                    }
                    node = node->left;
                }
            }
            else {
                if (node->right == nullptr) {
                    pos = iterator { node->next };
                    return node->right;
                }
                else {
                    if (key == node->right->key) {
                        pos = iterator { node->right };
                        return node->right;
                    }
                    node = node->right;
                }
            }
        }
    }

    IntrusiveList<T> elements_;
    base_node root_ {};
};

} // namespace exios

#endif // EXIOS_INTRUSIVE_MULTIMAP_HPP_INCLUDED
