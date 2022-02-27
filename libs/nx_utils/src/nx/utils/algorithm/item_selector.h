// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <chrono>
#include <optional>
#include <map>
#include <limits>
#include <vector>

#include <nx/utils/time.h>

namespace nx::utils::algorithm {

/**
 * Helps choosing an element from a set based on priorities that can be changed dynamically.
 *
 * Example: let us have a set of network connection methods. Different methods have different
 * implications. So, some are more preferrable than others. At the same time, not every connection
 * method works at all times. And the set of working connection methods may change in time.
 * <pre><code>
 * const auto popupRate = std::chrono::seconds(1);
 * ItemSelector<Name> selector(popupRate);
 *
 * // First, we register connection methods in the order of their preference:
 * selector.add("the most preferred method", 1);
 * selector.add("so-so method", 1);
 * selector.add("undesirable method, but better than nothing", 1);
 * // All items have depth 0.
 *
 * // When we need to choose a connection method we invoke
 * const auto choice1 = selector.topItem();
 * // It returns the item with minimal depth and the first added item among items having the same depth.
 * // The first item ("the most preferred method") is returned.
 *
 * // We establish connection. If it was not successful, then
 * selector.sinkItem(choice1, 100);
 * // The element identified by "choice1" is sunk to the depth of 100.
 * // It will take it 100 * weight (1) * popupRate seconds to go to depth 0.
 *
 * const auto choice2 = selector.topItem();
 * // The second ("so-so method") is chosen at this time since it is the first one that can be
 * // found on depth 0.
 * // ...
 * // If all items were sunk at some point, then a call to selector.topItem() will return the item
 * // with the minimal depth.
 * </code></pre>
 *
 * NOTE: Current implementation has linear complexity.
 * When needed, an implementation with logarithmic complexity could be provided
 * (e.g., based on max heap).
 */
template<typename ItemKey>
// requires Sortable<ItemKey> && Equality_comparable<ItemKey>
class ItemSelector
{
public:
    /**
     * @param popupTimePerWeightUnit The time it takes for one weight unit to pop up to 1 depth unit.
     * E.g., given speed on 1 weight unit per second it will take:
     * - 1 second  for key of weight 1 to popup from depth 1
     * - 2 seconds for key of weight 1 to popup from depth 2
     * - 2 seconds for key of weight 2 to popup from depth 1
     */
    ItemSelector(std::chrono::milliseconds popupTimePerWeightUnit);

    std::chrono::milliseconds popupTimePerWeightUnit() const;

    /**
     * By default, max depth has no limit.
     */
    void setMaxSinkDepth(unsigned int depth);

    /**
     * Set the max level an item can be ascended above the surface with ascendItem call.
     * By default, 0.
     * NOTE: Regardless of this setting items pop up automatically to the surface only.
     */
    void setMaxAscendLevel(unsigned int level);

    /**
     * @param key MUST be unique
     * @param weight Item's weight. The greater this weight, the lower the lifting speed.
     * @param minDepth Minimal depth the item can be lifted to.
     * @return false if key is not unique.
     */
    bool add(ItemKey key, int weight, int minDepth = 0);

    /**
     * @return ItemKey that is closest to the surface.
     * It every key is on the surface / same depth, then first added key is returned.
     */
    ItemKey topItem() const;

    /**
     * @return All top items. That means all items on the minimal depth.
     */
    std::vector<ItemKey> topItemRange() const;

    /**
     * Increase item's depth by the value specified.
     */
    void sinkItem(const ItemKey& key, unsigned int depth);

    /**
     * Ascend items by the given height.
     * An item may be ascended above the surface if setMaxAscendLevel was called.
     */
    void ascendItem(const ItemKey& key, unsigned int height);

    /**
     * @return std::nullopt if item not found by key.
     * Otherwise, a positive value specifies depth of a sunk item.
     * A negative value specifies height above the surface.
     */
    std::optional<int> itemDepth(const ItemKey& key) const;

    void clear();

private:
    struct Item
    {
        const int weight = 0;
        /** "0 depth" is surface. Negative value means above the surface. */
        int currentDepth = 0;
        const int minDepth = 0;
        std::chrono::steady_clock::time_point lastUpdateTime;
    };

    const std::chrono::milliseconds m_popupTimePerWeightUnit;
    unsigned int m_maxSinkDepth = std::numeric_limits<int>::max();
    int m_maxAscendLevel = 0;
    std::map<ItemKey, Item> m_items;

    void popupItems();

    void popupItem(
        std::chrono::steady_clock::time_point currentTime,
        Item* item);
};

//-------------------------------------------------------------------------------------------------

template<typename ItemKey>
ItemSelector<ItemKey>::ItemSelector(
    std::chrono::milliseconds popupTimePerWeightUnit)
    :
    m_popupTimePerWeightUnit(popupTimePerWeightUnit)
{
}

template<typename ItemKey>
std::chrono::milliseconds ItemSelector<ItemKey>::popupTimePerWeightUnit() const
{
    return m_popupTimePerWeightUnit;
}

template<typename ItemKey>
void ItemSelector<ItemKey>::setMaxSinkDepth(unsigned int depth)
{
    m_maxSinkDepth = depth;
}

template<typename ItemKey>
void ItemSelector<ItemKey>::setMaxAscendLevel(unsigned int level)
{
    m_maxAscendLevel = (int) level;
}

template<typename ItemKey>
bool ItemSelector<ItemKey>::add(ItemKey key, int weight, int minDepth)
{
    if (weight <= 0)
        return false;

    return m_items.emplace(
        std::move(key),
        Item{weight, minDepth, minDepth, monotonicTime()}).second;
}

template<typename ItemKey>
ItemKey ItemSelector<ItemKey>::topItem() const
{
    const_cast<ItemSelector*>(this)->popupItems();

    return std::min_element(
        m_items.begin(), m_items.end(),
        [](auto& left, auto& right)
        {
            return left.second.currentDepth < right.second.currentDepth;
        })->first;
}

template<typename ItemKey>
std::vector<ItemKey> ItemSelector<ItemKey>::topItemRange() const
{
    const_cast<ItemSelector*>(this)->popupItems();

    std::vector<ItemKey> result;
    std::optional<int> minDepth;
    for (const auto& [key, item]: m_items)
    {
        if (minDepth && *minDepth < item.currentDepth)
            continue;

        if (minDepth && *minDepth == item.currentDepth)
        {
            result.push_back(key);
            continue;
        }

        minDepth = item.currentDepth;
        result = {key};
    }

    return result;
}

template<typename ItemKey>
void ItemSelector<ItemKey>::sinkItem(const ItemKey& key, unsigned int depth)
{
    const_cast<ItemSelector*>(this)->popupItems();

    auto it = m_items.find(key);
    if (it == m_items.end())
        return;

    it->second.currentDepth = std::min<int>(
        it->second.currentDepth + (int) depth,
        m_maxSinkDepth);
    it->second.lastUpdateTime = monotonicTime();
}

template<typename ItemKey>
void ItemSelector<ItemKey>::ascendItem(const ItemKey& key, unsigned int height)
{
    const_cast<ItemSelector*>(this)->popupItems();

    if (auto it = m_items.find(key); it != m_items.end())
    {
        it->second.currentDepth = std::max<int>(
            it->second.currentDepth - (int) height, -m_maxAscendLevel);
    }
}

template<typename ItemKey>
std::optional<int> ItemSelector<ItemKey>::itemDepth(const ItemKey& key) const
{
    const_cast<ItemSelector*>(this)->popupItems();

    if (auto it = m_items.find(key); it != m_items.end())
        return it->second.currentDepth;

    return std::nullopt;
}

template<typename ItemKey>
void ItemSelector<ItemKey>::clear()
{
    m_items.clear();
}

template<typename ItemKey>
void ItemSelector<ItemKey>::popupItems()
{
    const auto currentTime = monotonicTime();

    for (auto& [key, item]: m_items)
    {
        if (item.currentDepth <= item.minDepth)
            continue; //< Item is already on surface.

        popupItem(currentTime, &item);
    }
}

template<typename ItemKey>
void ItemSelector<ItemKey>::popupItem(
    std::chrono::steady_clock::time_point currentTime,
    ItemSelector<ItemKey>::Item* item)
{
    using namespace std::chrono;

    const auto timePassed = duration_cast<milliseconds>(
        currentTime - item->lastUpdateTime);

    const auto distance = (timePassed / m_popupTimePerWeightUnit) / item->weight;

    if ((item->currentDepth - item->minDepth) > distance)
    {
        item->currentDepth -= distance;
        // Compensating round error in the previous line.
        item->lastUpdateTime += distance * item->weight * m_popupTimePerWeightUnit;
    }
    else
    {
        item->currentDepth = item->minDepth;
        item->lastUpdateTime = currentTime;
    }
}

} // nx::utils::algorithm
