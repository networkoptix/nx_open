// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <core/resource/layout_resource.h>
#include <core/resource_access/helpers/layout_item_aggregator.h>

class QnLayoutItemAggregatorTest: public testing::Test
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        aggregator.reset(new QnLayoutItemAggregator());
        QObject::connect(aggregator.data(), &QnLayoutItemAggregator::itemAdded,
            [this](const nx::Uuid& resourceId)
            {
                if (!m_awaiting)
                    return;

                ASSERT_TRUE(m_awaitedAddedItems.contains(resourceId));
                m_awaitedAddedItems.remove(resourceId);
            });
        QObject::connect(aggregator.data(), &QnLayoutItemAggregator::itemRemoved,
            [this](const nx::Uuid& resourceId)
            {
                if (!m_awaiting)
                    return;

                ASSERT_TRUE(m_awaitedRemovedItems.contains(resourceId));
                m_awaitedRemovedItems.remove(resourceId);
            });
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        aggregator.reset();
        if (m_awaiting)
        {
            ASSERT_TRUE(m_awaitedAddedItems.isEmpty());
            ASSERT_TRUE(m_awaitedRemovedItems.isEmpty());
        }
    }

    QnLayoutResourcePtr createLayout(const nx::Uuid& id = nx::Uuid::createUuid())
    {
        QnLayoutResourcePtr layout(new QnLayoutResource());
        layout->setIdUnsafe(id);
        return layout;
    }

    nx::Uuid addItem(const QnLayoutResourcePtr& layout, const nx::Uuid& resourceId = nx::Uuid::createUuid())
    {
        nx::vms::common::LayoutItemData item;
        item.uuid = nx::Uuid::createUuid();
        item.resource.id = resourceId;
        layout->addItem(item);
        return resourceId;
    }

    void awaitItemAdded(const nx::Uuid& resourceId)
    {
        m_awaiting = true;
        m_awaitedAddedItems.insert(resourceId);
    }

    void awaitItemRemoved(const nx::Uuid& resourceId)
    {
        m_awaiting = true;
        m_awaitedRemovedItems.insert(resourceId);
    }

    void setAwaiting(bool value)
    {
        m_awaiting = value;
    }

    QScopedPointer<QnLayoutItemAggregator> aggregator;

private:
    bool m_awaiting = false;
    QSet<nx::Uuid> m_awaitedAddedItems;
    QSet<nx::Uuid> m_awaitedRemovedItems;
};

TEST_F(QnLayoutItemAggregatorTest, checkInit)
{
    ASSERT_TRUE(aggregator->watchedLayouts().isEmpty());
}

TEST_F(QnLayoutItemAggregatorTest, checkAddLayout)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    ASSERT_TRUE(aggregator->hasLayout(layout));
}

TEST_F(QnLayoutItemAggregatorTest, checkRemoveLayout)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    aggregator->removeWatchedLayout(layout);
    ASSERT_FALSE(aggregator->hasLayout(layout));
}

TEST_F(QnLayoutItemAggregatorTest, checkLayoutItem)
{
    auto layout = createLayout();
    auto targetId = addItem(layout);
    aggregator->addWatchedLayout(layout);
    ASSERT_TRUE(aggregator->hasItem(targetId));
}

TEST_F(QnLayoutItemAggregatorTest, checkLayoutItemAbsent)
{
    auto targetId = nx::Uuid::createUuid();
    ASSERT_FALSE(aggregator->hasItem(targetId));
}

TEST_F(QnLayoutItemAggregatorTest, checkLayoutItemRemoved)
{
    auto targetId = nx::Uuid::createUuid();
    auto layout = createLayout();

    nx::vms::common::LayoutItemData item;
    item.uuid = nx::Uuid::createUuid();
    item.resource.id = targetId;
    layout->addItem(item);

    aggregator->addWatchedLayout(layout);
    layout->removeItem(item.uuid);
    ASSERT_FALSE(aggregator->hasItem(targetId));
}

TEST_F(QnLayoutItemAggregatorTest, checkLayoutItemAdded)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    auto targetId = addItem(layout);
    ASSERT_TRUE(aggregator->hasItem(targetId));
}

TEST_F(QnLayoutItemAggregatorTest, checkNullIdIgnored)
{
    auto layout = createLayout();
    addItem(layout, nx::Uuid());
    aggregator->addWatchedLayout(layout);
    ASSERT_FALSE(aggregator->hasItem(nx::Uuid()));
}

TEST_F(QnLayoutItemAggregatorTest, checkItemIfLayoutRemoved)
{
    auto layout = createLayout();
    auto targetId = addItem(layout);
    aggregator->addWatchedLayout(layout);
    aggregator->removeWatchedLayout(layout);
    ASSERT_FALSE(aggregator->hasItem(targetId));
}

TEST_F(QnLayoutItemAggregatorTest, stopListeningLayout)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    aggregator->removeWatchedLayout(layout);
    auto targetId = addItem(layout);
    ASSERT_FALSE(aggregator->hasItem(targetId));
}

TEST_F(QnLayoutItemAggregatorTest, addSeveralLayoutItems)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    auto targetId = addItem(layout);
    addItem(layout, targetId); /*< The same resource second time. */
    auto firstItemId = layout->getItems().begin().key();
    layout->removeItem(firstItemId);
    ASSERT_TRUE(aggregator->hasItem(targetId));
}

TEST_F(QnLayoutItemAggregatorTest, watchLayoutSeveralTimesResult)
{
    auto layout = createLayout();
    ASSERT_TRUE(aggregator->addWatchedLayout(layout));
    ASSERT_FALSE(aggregator->addWatchedLayout(layout));
    ASSERT_TRUE(aggregator->removeWatchedLayout(layout));
    ASSERT_FALSE(aggregator->removeWatchedLayout(layout));
}

TEST_F(QnLayoutItemAggregatorTest, watchLayoutSeveralTimes)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    aggregator->addWatchedLayout(layout);
    aggregator->removeWatchedLayout(layout);
    auto targetId = addItem(layout);
    ASSERT_FALSE(aggregator->hasItem(targetId));
}

TEST_F(QnLayoutItemAggregatorTest, awaitItemAdded)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    auto targetId = nx::Uuid::createUuid();
    awaitItemAdded(targetId);
    addItem(layout, targetId);
}

TEST_F(QnLayoutItemAggregatorTest, awaitItemAddedAtOnce)
{
    auto layout = createLayout();
    auto targetId = addItem(layout);
    awaitItemAdded(targetId);
    aggregator->addWatchedLayout(layout);
}

TEST_F(QnLayoutItemAggregatorTest, doNotRepeatItemAdded)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    auto targetId = nx::Uuid::createUuid();
    awaitItemAdded(targetId);
    addItem(layout, targetId);
    addItem(layout, targetId);
}

TEST_F(QnLayoutItemAggregatorTest, awaitItemRemoved)
{
    auto layout = createLayout();
    auto targetId = addItem(layout);
    aggregator->addWatchedLayout(layout);

    awaitItemRemoved(targetId);
    layout->setItems(nx::vms::common::LayoutItemDataList{});
}

TEST_F(QnLayoutItemAggregatorTest, ignoreNullIdItemRemoved)
{
    auto layout = createLayout();
    addItem(layout, nx::Uuid());
    aggregator->addWatchedLayout(layout);
    setAwaiting(true);
    layout->setItems(nx::vms::common::LayoutItemDataList{});
}

TEST_F(QnLayoutItemAggregatorTest, awaitItemRemovedOnRemovingLayout)
{
    auto layout = createLayout();
    auto targetId = addItem(layout);
    aggregator->addWatchedLayout(layout);

    awaitItemRemoved(targetId);
    aggregator->removeWatchedLayout(layout);
}

TEST_F(QnLayoutItemAggregatorTest, ignoreNullIdOnLayoutRemoved)
{
    auto layout = createLayout();
    addItem(layout, nx::Uuid());
    aggregator->addWatchedLayout(layout);
    setAwaiting(true);
    aggregator->removeWatchedLayout(layout);
}

TEST_F(QnLayoutItemAggregatorTest, awaitItemRemovedOnce)
{
    auto layout = createLayout();
    auto targetId = addItem(layout);
    addItem(layout, targetId); /*< Second time */
    aggregator->addWatchedLayout(layout);
    setAwaiting(true);
    auto firstItemId = layout->getItems().begin().key();
    layout->removeItem(firstItemId);
}

TEST_F(QnLayoutItemAggregatorTest, iterateOverLayouts)
{
    static const int N = 15;
    for (int i = 0; i < N; ++i)
        aggregator->addWatchedLayout(createLayout());

    ASSERT_EQ(N, aggregator->watchedLayouts().size());
}
