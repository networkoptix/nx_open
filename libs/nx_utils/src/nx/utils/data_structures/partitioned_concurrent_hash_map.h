// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include <nx/utils/thread/mutex.h>

namespace nx::utils {

/**
 * Safe hash map that protects parts of its data by multiple mutexes.
 * Using multiple mutexes helps to reduce waits by threads accessing this hashmap.
 * Internally, a collection of fixed number of hash maps each protected by its own mutex.
 */
template<
    typename Key,
    typename T,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>>
class PartitionedConcurrentHashMap
{
public:
    using HashMap = std::unordered_map<Key, T, Hash, KeyEqual>;
    using key_type = typename HashMap::key_type;
    using mapped_type = typename HashMap::mapped_type;
    using value_type = typename HashMap::value_type;
    using size_type = typename HashMap::size_type;
    using hasher = typename HashMap::hasher;

    /**
     * @param partitionCount If zero, then initialized with 2 * std::thread::hardware_concurrency();
     */
    PartitionedConcurrentHashMap(unsigned int partitionCount = 0);

    bool empty() const;
    size_type size() const;

    bool insert(const value_type& value);
    bool insert(value_type&& value);
    bool insert_or_assign(const Key& key, const T& value);
    bool insert_or_assign(Key&& key, T&& value);
    template<typename... Args> bool emplace(Args&&... args);
    size_type erase(const Key& key);

    /** Removes and returns element from the dictionary. */
    std::optional<T> take(const Key& key);

    /** Aggregates all elements into a single dictionary and returns it. */
    HashMap takeAll();

    void clear();

    std::optional<T> find(const Key& key) const;
    bool contains(const Key& key) const;

    /**
     * Invokes func on the value associated with the given key.
     * If no such element exists, it is inserted first.
     */
    template<typename Func>
    // requires std::is_invocable_v<Func, mapped_type>
    void modify(const Key& key, Func func);

    /**
     * Invokes func on the value associated with the given key.
     * If no such element exists, it returns false.
     */
    template<typename Func>
    // requires std::is_invocable_v<Func, mapped_type>
    bool modifyExisting(const Key& key, Func func);

    /**
     * Invokes func for every element.
     * func is invoked with internal mutex locked.
     * Each partition's mutex is locked once for each partition, not per element!
     */
    template<typename Func>
    // requires std::is_invocable_v<Func, value_type>
    void forEach(Func func);

    template<typename Func>
    // requires std::is_invocable_v<Func, const value_type&>
    void forEach(Func func) const;

private:
    size_type partition(const key_type& key) const;

private:
    struct HashMapCtx
    {
        HashMap dict;
        mutable nx::ReadWriteLock mtx;
    };

    std::vector<HashMapCtx> m_partitions;
    std::atomic<size_type> m_size{0};
};

template<typename Key, typename T, typename Hash, typename KeyEqual>
PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::PartitionedConcurrentHashMap(
    unsigned int partitionCount)
{
    m_partitions.resize(
        partitionCount > 0 ? partitionCount : 2 * std::thread::hardware_concurrency());
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
bool PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::empty() const
{
    return size() == 0;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
typename PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::size_type
    PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::size() const
{
    return m_size.load();
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
bool PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::insert(const value_type& value)
{
    auto& ctx = m_partitions[partition(value.first)];
    NX_WRITE_LOCKER locker(&ctx.mtx);

    const bool inserted = ctx.dict.insert(value).second;
    if (inserted)
        ++m_size;
    return inserted;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
bool PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::insert(value_type&& value)
{
    auto& ctx = m_partitions[partition(value.first)];
    NX_WRITE_LOCKER locker(&ctx.mtx);

    const bool inserted = ctx.dict.insert(std::move(value)).second;
    if (inserted)
        ++m_size;
    return inserted;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
bool PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::insert_or_assign(
    const Key& key,
    const T& value)
{
    auto& ctx = m_partitions[partition(key)];
    NX_WRITE_LOCKER locker(&ctx.mtx);

    const bool inserted = ctx.dict.insert_or_assign(key, value).second;
    if (inserted)
        ++m_size;
    return inserted;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
bool PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::insert_or_assign(Key&& key, T&& value)
{
    auto& ctx = m_partitions[partition(key)];
    NX_WRITE_LOCKER locker(&ctx.mtx);

    const bool inserted = ctx.dict.insert_or_assign(std::move(key), std::move(value)).second;
    if (inserted)
        ++m_size;
    return inserted;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
template<typename... Args>
bool PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::emplace(Args&&... args)
{
    value_type val(std::forward<Args>(args)...);
    auto& ctx = m_partitions[partition(val.first)];
    NX_WRITE_LOCKER locker(&ctx.mtx);

    const bool inserted = ctx.dict.insert(std::move(val)).second;
    if (inserted)
        ++m_size;
    return inserted;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
typename PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::size_type
    PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::erase(const Key& key)
{
    auto& ctx = m_partitions[partition(key)];
    NX_WRITE_LOCKER locker(&ctx.mtx);

    const auto erasedCount = ctx.dict.erase(key);
    m_size -= erasedCount;
    return erasedCount;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
std::optional<T> PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::take(const Key& key)
{
    auto& ctx = m_partitions[partition(key)];
    NX_WRITE_LOCKER locker(&ctx.mtx);

    auto it = ctx.dict.find(key);
    if (it == ctx.dict.end())
        return std::nullopt;

    auto val = std::move(it->second);
    ctx.dict.erase(it);
    --m_size;
    return val;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
typename PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::HashMap
    PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::takeAll()
{
    HashMap all;
    for (auto& ctx: m_partitions)
    {
        NX_WRITE_LOCKER locker(&ctx.mtx);
        std::move(ctx.dict.begin(), ctx.dict.end(), std::inserter(all, all.end()));
        m_size -= ctx.dict.size();
        ctx.dict.clear();
    }

    return all;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
void PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::clear()
{
    std::for_each(
        m_partitions.begin(),
        m_partitions.end(),
        [this](auto& ctx)
        {
            NX_WRITE_LOCKER locker(&ctx.mtx);
            m_size -= ctx.dict.size();
            ctx.dict.clear();
        });
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
std::optional<T> PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::find(const Key& key) const
{
    const auto& ctx = m_partitions[partition(key)];
    NX_READ_LOCKER locker(&ctx.mtx);

    auto it = ctx.dict.find(key);
    if (it != ctx.dict.end())
        return it->second;
    return std::nullopt;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
bool PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::contains(const Key& key) const
{
    const auto& ctx = m_partitions[partition(key)];
    NX_READ_LOCKER locker(&ctx.mtx);

    return ctx.dict.contains(key);
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
template<typename Func>
void PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::modify(const Key& key, Func func)
{
    auto& ctx = m_partitions[partition(key)];
    NX_WRITE_LOCKER locker(&ctx.mtx);

    func(ctx.dict[key]);
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
template<typename Func>
bool PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::modifyExisting(
    const Key& key,
    Func func)
{
    auto& ctx = m_partitions[partition(key)];
    NX_WRITE_LOCKER locker(&ctx.mtx);

    auto it = ctx.dict.find(key);
    if (it == ctx.dict.end())
        return false;
    func(it->second);
    return true;
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
template<typename Func>
void PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::forEach(Func func)
{
    std::for_each(
        m_partitions.begin(),
        m_partitions.end(),
        [&func](auto& ctx)
        {
            NX_WRITE_LOCKER locker(&ctx.mtx);
            std::for_each(ctx.dict.begin(), ctx.dict.end(), func);
        });
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
template<typename Func>
void PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::forEach(Func func) const
{
    std::for_each(
        m_partitions.begin(),
        m_partitions.end(),
        [&func](const auto& ctx)
        {
            NX_READ_LOCKER locker(&ctx.mtx);
            std::for_each(ctx.dict.cbegin(), ctx.dict.cend(), func);
        });
}

template<typename Key, typename T, typename Hash, typename KeyEqual>
typename PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::size_type
    PartitionedConcurrentHashMap<Key, T, Hash, KeyEqual>::partition(const key_type& key) const
{
    return hasher()(key) % m_partitions.size();
}

} // namespace nx::utils
