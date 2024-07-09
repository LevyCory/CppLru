#pragma once

#include <algorithm>
#include <concepts>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

template <
    typename Key,
    typename Value,
    typename Hash = std::hash<Key>,
    typename Eq = std::equal_to<const Key>,
    template <typename> typename ListAllocator = std::allocator,
    template <typename> typename MapAllocator = std::allocator
>
class lru
{
public:
    using key_type = Key;
    using value_type = Value;
    using value_type_ref = std::reference_wrapper<value_type>;
    using key_type_ref = std::reference_wrapper<const key_type>;
    using hash_type = Hash;
    using key_value_pair = std::pair<const key_type, value_type>;
    using list_allocator = ListAllocator<key_value_pair>;
    using list_type = std::list<key_value_pair, list_allocator>;
    using iterator = typename list_type::iterator;
    using const_iterator = typename list_type::const_iterator;
    using map_allocator = MapAllocator<std::pair<const key_type_ref, iterator>>;
    using map_type = std::unordered_map<key_type_ref, iterator, Hash, Eq, map_allocator>;

    explicit lru(size_t capacity) :
        _capacity(capacity)
    {}

    iterator end() noexcept { return _list.end(); }
    iterator begin() noexcept { return _list.begin(); }
    const_iterator begin() const noexcept { return _list.begin(); }
    const_iterator end() const noexcept { return _list.end(); }
    const_iterator cbegin() const noexcept { return _list.cbegin(); }
    const_iterator cend() const noexcept { return _list.cend(); }

    bool empty() const noexcept { return _list.empty(); }
    size_t size() const noexcept { return _list.size(); }
    size_t max_size() const noexcept { return _capacity; }

    bool contains(const key_type& key)
    {
        return find(key) != end();
    }

    template <typename K, typename V>
    bool insert(K&& key, V&& value)
    {
        if (_contains(key))
        {
            return false;
        }

        _insert(std::forward<K>(key), std::forward<V>(value));
        return true;
    }

    template <typename K, typename V>
    bool insert_or_assign(K&& key, V&& value)
    {
        auto map_iter = _map.find(key);
        if (map_iter != _map.end())
        {
            map_iter->second->second = std::forward<V>(value);
            _promote(map_iter->second);
            return false;
        }

        _insert(std::forward<K>(key), std::forward<V>(value));
        return true;
    }

    iterator find(const key_type& key)
    {
        auto iter = _map.find(key);
        if (iter == _map.end())
        {
            return end();
        }

        _promote(iter->second);
        return iter->second;
    }

    void resize(size_t capacity)
    {
        _prune(capacity);
        _capacity = capacity;
    }

    void for_each(std::invocable<key_value_pair&> auto func)
    {
        std::for_each(begin(), end(), func);
    }

    bool try_update(const key_type& key, std::invocable<value_type&> auto func)
    {
        auto iter = find(key);
        if (iter == end())
        {
            return false;
        }

        func(iter->second);
        return true;
    }

    std::optional<value_type_ref> get(const key_type& key)
    {
        auto iterator = _map.find(key);
        if (iterator == _map.end())
        {
            return std::nullopt;
        }

        _promote(iterator->second);
        return { iterator->second->second };
    }

    std::optional<value_type> get_copy(const key_type& key)
    {
        auto iterator = _map.find(key);
        if (iterator == _map.end())
        {
            return std::nullopt;
        }

        _promote(iterator->second);
        return { iterator->second->second };
    }

    void clear() noexcept
    {
        _map.clear();
        _list.clear();
    }

protected:
    template <typename K, typename V>
    void _insert(K&& key, V&& value)
    {
        auto iter = _list.emplace(_list.begin(),
                                  std::forward<K>(key),
                                  std::forward<V>(value));

        _map[iter->first] = _list.begin();
        _prune();
    }

    bool _contains(const key_type& key) const
    {
        return _map.find(key) != _map.end();
    }

    void _promote(iterator iter)
    {
        if (iter != _list.begin())
        {
            _list.splice(_list.begin(), _list, iter);
        }
    }

    void _evict()
    {
        _map.erase(_list.back().first);
        _list.pop_back();
    }

    void _prune(size_t capacity)
    {
        while (_list.size() > capacity)
        {
            _evict();
        }
    }

    void _prune() { _prune(_capacity); }

    map_type _map;
    list_type _list;
    size_t _capacity;
};
