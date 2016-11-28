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

    QnLayoutResourcePtr createLayout(const QnUuid& id)
    {
        QnLayoutResourcePtr layout(new QnLayoutResource());
        layout->setId(id);
        return layout;
    }

    QScopedPointer<QnLayoutItemAggregator> aggregator;
};

TEST_F(QnLayoutItemAggregatorTest, checkInit)
{
    ASSERT_TRUE(aggregator->watchedLayouts().isEmpty());
}
