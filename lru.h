#ifndef __lru_h__
#define __lru_h__

#include <algorithm>
#include <functional>
#include <list>
#include <mutex>
#include <unordered_map>
#include <utility>

template <
    typename Key,
    typename Value,
    typename Hash = std::hash<Key>,
    typename Eq = std::equal_to<const Key>>
class lru
{
public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const key_type, mapped_type>;
    using ref_wrapped_key_type = std::reference_wrapper<const Key>;
    using hash_type = Hash;
    using list_type = std::list<value_type>;
    using iterator = typename list_type::iterator;
    using const_iterator = typename list_type::const_iterator;
    using map_type = std::unordered_map<ref_wrapped_key_type, iterator, Hash, Eq>;

    explicit lru(std::size_t capacity) :
        _capacity(capacity)
    {}

    // Iterators
    iterator begin() noexcept { return _list.begin(); }
    const_iterator begin() const noexcept { return _list.begin(); }
    const_iterator cbegin() const noexcept { return _list.cbegin(); }
    iterator end() noexcept { return _list.end(); }
    const_iterator end() const noexcept { return _list.end(); }
    const_iterator cend() const noexcept { return _list.cend(); }

    // Size
    bool empty() const noexcept { return _list.empty(); }
    std::size_t size() const noexcept { return _list.size(); }
    std::size_t max_size() const noexcept { return _capacity; }

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
        auto map_iter = _map.find(std::cref(key));
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
        auto iter = _map.find(std::cref(key));
        if (iter == _map.end())
        {
            return end();
        }

        _promote(iter->second);
        return iter->second;
    }

    void resize(std::size_t capacity)
    {
        _prune(capacity);
        _capacity = capacity;
    }

    void for_each(std::function<void(value_type&)> func)
    {
        std::for_each(begin(), end(), func);
    }

    bool try_update(const key_type& key, std::function<void(mapped_type&)> func)
    {
        auto iter = find(key);
        if (iter == end())
        {
            return false;
        }

        func(iter->second);
        return true;
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

        _map[std::cref(iter->first)] = _list.begin();
        _prune();
    }

    bool _contains(const key_type& key) const
    {
        return _map.find(std::cref(key)) != _map.end();
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
        _map.erase(std::cref(_list.back().first));
        _list.pop_back();
    }

    void _prune(std::size_t capacity)
    {
        while (_list.size() > capacity)
        {
            _evict();
        }
    }

    void _prune() { _prune(_capacity); }

    map_type _map;
    list_type _list;
    std::size_t _capacity;
};

#endif /* __lru_h__ */
