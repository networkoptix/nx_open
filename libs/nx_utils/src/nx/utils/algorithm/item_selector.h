#pragma once

#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>

#include <nx/utils/time.h>

namespace nx::utils::algorithm {

/**
 * NOTE: Current implementation is linear. 
 * When needed, logarithmic complexity based implementation could be provided
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
     * - 1 second  for itemKey of weight 1 to popup from depth 1
     * - 2 seconds for itemKey of weight 1 to popup from depth 2
     * - 2 seconds for itemKey of weight 2 to popup from depth 1
     */
    ItemSelector(std::chrono::milliseconds popupTimePerWeightUnit);

    std::chrono::milliseconds popupTimePerWeightUnit() const;

    /**
     * By default, max depth has no limit.
     */
    void setMaxSinkDepth(int depth);

    /**
     * @param itemKey MUST be unique
     * @param maxSinkDepth It is common to make it equal to weight
     * @return false if itemKey is not unique
     */
    bool add(ItemKey itemKey, int weight);

    /**
     * @return ItemKey that is closest to the surface.
     * It every itemKey is on the surface / same depth, then first added itemKey is returned.
     */
    ItemKey topItem() const;

    void sinkItem(const ItemKey& itemKey, int depth);

    void clear();

private:
    struct Item
    {
        ItemKey itemKey = ItemKey();
        const int weight = 0;
        /** "0 depth" is surface. */
        int currentDepth = 0;
        std::chrono::steady_clock::time_point lastUpdateTime;
    };

    const std::chrono::milliseconds m_popupTimePerWeightUnit;
    int m_maxSinkDepth = std::numeric_limits<int>::max();
    std::vector<Item> m_items;

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
void ItemSelector<ItemKey>::setMaxSinkDepth(int depth)
{
    m_maxSinkDepth = depth;
}

template<typename ItemKey>
bool ItemSelector<ItemKey>::add(ItemKey itemKey, int weight)
{
    if (weight <= 0)
        return false;

    if (std::any_of(
            m_items.begin(), m_items.end(),
            [&itemKey](const Item& ctx) { return ctx.itemKey == itemKey; }))
    {
        return false;
    }

    m_items.push_back({std::move(itemKey), weight, 0, monotonicTime()});

    return true;
}

template<typename ItemKey>
ItemKey ItemSelector<ItemKey>::topItem() const
{
    const_cast<ItemSelector*>(this)->popupItems();

    return std::min_element(
        m_items.begin(), m_items.end(),
        [](auto& left, auto& right) { return left.currentDepth < right.currentDepth; })->itemKey;
}

template<typename ItemKey>
void ItemSelector<ItemKey>::sinkItem(const ItemKey& itemKey, int depth)
{
    const_cast<ItemSelector*>(this)->popupItems();

    for (auto& item: m_items)
    {
        if (item.itemKey != itemKey)
            continue;

        item.currentDepth = std::min(
            item.currentDepth + depth,
            m_maxSinkDepth);
        item.lastUpdateTime = monotonicTime();
    }
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

    for (auto& item: m_items)
    {
        if (item.currentDepth == 0)
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

    const int distance = (timePassed / m_popupTimePerWeightUnit) / item->weight;

    if (item->currentDepth > distance)
    {
        item->currentDepth -= distance;
        // Compensating round error in the previous line.
        item->lastUpdateTime += distance * item->weight * m_popupTimePerWeightUnit;
    }
    else
    {
        item->currentDepth = 0;
        item->lastUpdateTime = currentTime;
    }
}

} // nx::utils::algorithm
