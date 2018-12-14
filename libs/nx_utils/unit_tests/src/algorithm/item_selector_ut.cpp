#include <gtest/gtest.h>

#include <nx/utils/algorithm/item_selector.h>
#include <nx/utils/random.h>
#include <nx/utils/time.h>

namespace nx::utils::algorithm::test {

class ItemSelector:
    public ::testing::Test
{
public:
    ItemSelector():
        m_selector(std::chrono::seconds(1)),
        m_timeShift(utils::test::ClockType::steady)
    {
    }

protected:
    struct Item
    {
        int key = 0;
        int weight = 0;
        int currentDepth = 0;
    };
    
    enum class ItemOrder
    {
        ascendingWeight,
        descendingWeight,
    };

    void addItems(int itemCount, ItemOrder itemOrder)
    {
        m_items.resize(itemCount);
        for (int i = 0; i < itemCount; ++i)
        {
            const auto weight = itemOrder == ItemOrder::ascendingWeight
                ? 3 * (i + 1)
                : 3 * (itemCount - i);

            m_items[i].key = i;
            m_items[i].weight = weight;
            ASSERT_TRUE(m_selector.add(i, weight));
        }
    }

    void givenGroupOfItems()
    {
        addItems(7, ItemOrder::ascendingWeight);
    }

    void givenTwoItemsOfDifferentWeight()
    {
        addItems(2, ItemOrder::descendingWeight);
    }

    void sinkItem(int key, int depth)
    {
        m_items[key].currentDepth += depth;
        m_selector.sinkItem(key, depth);
    }

    void sinkItemsToDifferentDepth()
    {
        for (int i = 0; i < (int) m_items.size(); ++i)
            sinkItem(i, random::number<int>(0, 37));
    }

    void sinkFirstItem()
    {
        sinkItem(0, 10);
    }

    Item frontItem()
    {
        return m_items.front();
    }

    void assertTheFirstItemIsSelected()
    {
        ASSERT_EQ(0, m_selector.topItem());
    }

    void assertTheClosestToTheSurfaceItemIsSelected()
    {
        ASSERT_EQ(closestToTheSurfaceItem().key, m_selector.topItem());
    }

    void assertTopItemIs(int key)
    {
        ASSERT_EQ(key, m_selector.topItem());
    }

    void assertTopItemIsNot(int key)
    {
        ASSERT_NE(key, m_selector.topItem());
    }

    void waitForTopItemToBe(int key)
    {
        while (m_selector.topItem() != key)
            m_timeShift.applyRelativeShift(std::chrono::milliseconds(1));
    }

    Item heaviestItem() const
    {
        return *std::max_element(
            m_items.begin(), m_items.end(),
            [](auto& left, auto& right) { return left.weight < right.weight; });
    }

    Item lightestItem() const
    {
        return *std::min_element(
            m_items.begin(), m_items.end(),
            [](auto& left, auto& right) { return left.weight < right.weight; });
    }

    Item closestToTheSurfaceItem() const
    {
        return *std::min_element(
            m_items.begin(), m_items.end(),
            [](auto& left, auto& right) { return left.currentDepth < right.currentDepth; });
    }
    
    std::chrono::milliseconds popupTimePerWeightUnit() const
    {
        return m_selector.popupTimePerWeightUnit();
    }

private:
    algorithm::ItemSelector<int> m_selector;
    std::vector<Item> m_items;
    utils::test::ScopedTimeShift m_timeShift;
};

TEST_F(ItemSelector, of_items_on_the_same_depth_the_first_one_is_selected)
{
    givenGroupOfItems();
    assertTheFirstItemIsSelected();
}

TEST_F(ItemSelector, closest_to_the_surface_item_is_selected)
{
    givenGroupOfItems();
    sinkItemsToDifferentDepth();
    assertTheClosestToTheSurfaceItemIsSelected();
}

TEST_F(ItemSelector, items_popup_with_different_speed)
{
    givenTwoItemsOfDifferentWeight();

    sinkItem(heaviestItem().key, heaviestItem().weight * 10);
    sinkItem(lightestItem().key, heaviestItem().weight * 15);

    assertTopItemIs(heaviestItem().key);

    waitForTopItemToBe(lightestItem().key);
}

TEST_F(ItemSelector, sunk_item_reaches_surface_eventually)
{
    givenGroupOfItems();

    sinkFirstItem();
    assertTopItemIsNot(frontItem().key);

    waitForTopItemToBe(frontItem().key);
}

} // namespace nx::utils::algorithm::test
