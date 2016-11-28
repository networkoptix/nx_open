#include <gtest/gtest.h>

#include <core/resource_access/helpers/layout_item_aggregator.h>

#include <core/resource/layout_resource.h>

class QnLayoutItemAggregatorTest: public testing::Test
{
protected:

    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        aggregator.reset(new QnLayoutItemAggregator());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        aggregator.reset();
    }

    QnLayoutResourcePtr createLayout(const QnUuid& id = QnUuid::createUuid())
    {
        QnLayoutResourcePtr layout(new QnLayoutResource());
        layout->setId(id);
        return layout;
    }

    QnUuid addItem(const QnLayoutResourcePtr& layout, const QnUuid& resourceId = QnUuid::createUuid())
    {
        QnLayoutItemData item;
        item.uuid = QnUuid::createUuid();
        item.resource.id = resourceId;
        layout->addItem(item);
        return resourceId;
    }

    QScopedPointer<QnLayoutItemAggregator> aggregator;
};

TEST_F(QnLayoutItemAggregatorTest, checkInit)
{
    ASSERT_TRUE(aggregator->watchedLayouts().isEmpty());
}

TEST_F(QnLayoutItemAggregatorTest, checkAddLayout)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    ASSERT_TRUE(aggregator->watchedLayouts().contains(layout));
}

TEST_F(QnLayoutItemAggregatorTest, checkRemoveLayout)
{
    auto layout = createLayout();
    aggregator->addWatchedLayout(layout);
    aggregator->removeWatchedLayout(layout);
    ASSERT_FALSE(aggregator->watchedLayouts().contains(layout));
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
    auto targetId = QnUuid::createUuid();
    ASSERT_FALSE(aggregator->hasItem(targetId));
}

TEST_F(QnLayoutItemAggregatorTest, checkLayoutItemRemoved)
{
    auto targetId = QnUuid::createUuid();
    auto layout = createLayout();

    QnLayoutItemData item;
    item.uuid = QnUuid::createUuid();
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
    addItem(layout, QnUuid());
    aggregator->addWatchedLayout(layout);
    ASSERT_FALSE(aggregator->hasItem(QnUuid()));
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
