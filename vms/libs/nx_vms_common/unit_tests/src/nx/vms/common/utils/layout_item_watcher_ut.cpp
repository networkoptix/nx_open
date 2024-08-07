// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <set>

#include <core/resource/layout_resource.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/common/utils/layout_item_watcher.h>

namespace nx::vms::common {
namespace test {

class LayoutItemWatcherTest: public ContextBasedTest
{
public:
    virtual void SetUp() override
    {
        ContextBasedTest::SetUp();
        watcher.reset(new LayoutItemWatcher());

        QObject::connect(watcher.get(), &LayoutItemWatcher::addedToLayout,
            [this](const nx::Uuid& resourceId, const QnLayoutResourcePtr& layout)
            {
                lastSignals.push_back("addedToLayout");
                lastResourceIds.push_back(resourceId);
                lastLayouts.push_back(layout);
            });

        QObject::connect(watcher.get(), &LayoutItemWatcher::removedFromLayout,
            [this](const nx::Uuid& resourceId, const QnLayoutResourcePtr& layout)
            {
                lastSignals.push_back("removedFromLayout");
                lastResourceIds.push_back(resourceId);
                lastLayouts.push_back(layout);
            });

        QObject::connect(watcher.get(), &LayoutItemWatcher::resourceAdded,
            [this](const nx::Uuid& resourceId)
            {
                lastSignals.push_back("resourceAdded");
                lastResourceIds.push_back(resourceId);
            });

        QObject::connect(watcher.get(), &LayoutItemWatcher::resourceRemoved,
            [this](const nx::Uuid& resourceId)
            {
                lastSignals.push_back("resourceRemoved");
                lastResourceIds.push_back(resourceId);
            });
    }

    virtual void TearDown() override
    {
        lastSignals.clear();
        lastLayouts.clear();
        lastResourceIds.clear();
        watcher.reset();
        ContextBasedTest::TearDown();
    }

    QString popNextSignal()
    {
        return lastSignals.empty() ? QString() : lastSignals.takeFirst();
    }

    nx::Uuid popNextResourceId()
    {
        return lastResourceIds.empty() ? nx::Uuid() : lastResourceIds.takeFirst();
    }

    QnLayoutResourcePtr popNextLayout()
    {
        return lastLayouts.empty() ? QnLayoutResourcePtr() : lastLayouts.takeFirst();
    }

    bool hasSignals() const
    {
        return !lastSignals.empty();
    }

    void clearSignals()
    {
        lastSignals.clear();
        lastLayouts.clear();
        lastResourceIds.clear();
    }

    using LayoutSet = std::set<QnLayoutResourcePtr>;
    LayoutSet toSet(const QnCounterHash<QnLayoutResourcePtr>& layouts)
    {
        return LayoutSet(layouts.keyBegin(), layouts.keyEnd());
    }

public:
    std::unique_ptr<LayoutItemWatcher> watcher;

private:
    QStringList lastSignals;
    QList<nx::Uuid> lastResourceIds;
    QList<QnLayoutResourcePtr> lastLayouts;
};

TEST_F(LayoutItemWatcherTest, isWatched)
{
    const auto layout1 = createLayout();
    const auto layout2 = createLayout();
    ASSERT_FALSE(watcher->isWatched(layout1));
    ASSERT_FALSE(watcher->isWatched(layout2));

    watcher->addWatchedLayout(layout1);
    ASSERT_TRUE(watcher->isWatched(layout1));
    ASSERT_FALSE(watcher->isWatched(layout2));

    watcher->addWatchedLayout(layout2);
    ASSERT_TRUE(watcher->isWatched(layout1));
    ASSERT_TRUE(watcher->isWatched(layout2));

    watcher->removeWatchedLayout(layout1);
    ASSERT_FALSE(watcher->isWatched(layout1));
    ASSERT_TRUE(watcher->isWatched(layout2));

    watcher->removeWatchedLayout(layout2);
    ASSERT_FALSE(watcher->isWatched(layout1));
    ASSERT_FALSE(watcher->isWatched(layout2));
}

TEST_F(LayoutItemWatcherTest, addRemoveResource)
{
    const auto layout1 = createLayout();
    const auto layout2 = createLayout();
    watcher->addWatchedLayout(layout1);
    watcher->addWatchedLayout(layout2);

    const auto camera = createCamera();
    ASSERT_FALSE(watcher->hasResource(camera->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera->getId())), LayoutSet());

    const auto item1 = addToLayout(layout1, camera);
    ASSERT_TRUE(watcher->hasResource(camera->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera->getId())), LayoutSet({layout1}));

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), camera->getId());
    ASSERT_EQ(popNextLayout(), layout1);

    ASSERT_EQ(popNextSignal(), QString("resourceAdded"));
    ASSERT_EQ(popNextResourceId(), camera->getId());

    const auto item2 = addToLayout(layout2, camera);
    ASSERT_TRUE(watcher->hasResource(camera->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera->getId())), LayoutSet({layout1, layout2}));

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), camera->getId());
    ASSERT_EQ(popNextLayout(), layout2);

    layout1->removeItem(item1);
    ASSERT_TRUE(watcher->hasResource(camera->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera->getId())), LayoutSet({layout2}));

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), camera->getId());
    ASSERT_EQ(popNextLayout(), layout1);

    layout2->removeItem(item2);
    ASSERT_FALSE(watcher->hasResource(camera->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera->getId())), LayoutSet());

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), camera->getId());
    ASSERT_EQ(popNextLayout(), layout2);

    ASSERT_EQ(popNextSignal(), QString("resourceRemoved"));
    ASSERT_EQ(popNextResourceId(), camera->getId());
}

TEST_F(LayoutItemWatcherTest, addRemoveLayout)
{
    const auto layout1 = createLayout();
    const auto layout2 = createLayout();
    const auto camera1 = createCamera();
    const auto camera2 = createCamera();
    addToLayout(layout1, camera1);
    addToLayout(layout1, camera2);
    addToLayout(layout2, camera1);
    addToLayout(layout2, camera2);

    // Layout items are not sorted, so we need to determine their order for further checks.
    const auto layout1_item1 = layout1->getItems().cbegin()->resource.id;
    const auto layout1_item2 = (++layout1->getItems().cbegin())->resource.id;
    const auto layout2_item1 = layout2->getItems().cbegin()->resource.id;
    const auto layout2_item2 = (++layout2->getItems().cbegin())->resource.id;

    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet());

    watcher->addWatchedLayout(layout1);
    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet({layout1}));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet({layout1}));

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), layout1_item1);
    ASSERT_EQ(popNextLayout(), layout1);

    ASSERT_EQ(popNextSignal(), QString("resourceAdded"));
    ASSERT_EQ(popNextResourceId(), layout1_item1);

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), layout1_item2);
    ASSERT_EQ(popNextLayout(), layout1);

    ASSERT_EQ(popNextSignal(), QString("resourceAdded"));
    ASSERT_EQ(popNextResourceId(), layout1_item2);

    watcher->addWatchedLayout(layout2);
    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet({layout1, layout2}));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet({layout1, layout2}));

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), layout2_item1);
    ASSERT_EQ(popNextLayout(), layout2);

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), layout2_item2);
    ASSERT_EQ(popNextLayout(), layout2);

    watcher->removeWatchedLayout(layout1);
    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet({layout2}));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet({layout2}));

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), layout1_item1);
    ASSERT_EQ(popNextLayout(), layout1);

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), layout1_item2);
    ASSERT_EQ(popNextLayout(), layout1);

    watcher->removeWatchedLayout(layout2);
    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet());

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), layout2_item1);
    ASSERT_EQ(popNextLayout(), layout2);

    ASSERT_EQ(popNextSignal(), QString("resourceRemoved"));
    ASSERT_EQ(popNextResourceId(), layout2_item1);

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), layout2_item2);
    ASSERT_EQ(popNextLayout(), layout2);

    ASSERT_EQ(popNextSignal(), QString("resourceRemoved"));
    ASSERT_EQ(popNextResourceId(), layout2_item2);
}

TEST_F(LayoutItemWatcherTest, layoutItemChanged)
{
    // Initialize watched layout with two items, with camera1 and camera2 respectively.

    const auto layout = createLayout();
    const auto camera1 = createCamera();
    const auto camera2 = createCamera();
    const auto camera3 = createCamera();
    const auto item1 = addToLayout(layout, camera1);
    const auto item2 = addToLayout(layout, camera2);

    watcher->addWatchedLayout(layout);
    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_FALSE(watcher->hasResource(camera3->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet({layout}));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet({layout}));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera3->getId())), LayoutSet());

    ASSERT_TRUE(hasSignals());
    clearSignals();

    auto items = layout->getItems();

    // Just swap item cameras.
    items[item1].resource.id = camera2->getId();
    items[item2].resource.id = camera1->getId();

    layout->setItems(items);

    // Check that nothing have changed.
    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_FALSE(watcher->hasResource(camera3->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet({layout}));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet({layout}));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera3->getId())), LayoutSet());

    // Depending on internal item order, there could have been add-remove or remove-add signals,
    // we don't check them here.
    ASSERT_TRUE(hasSignals());
    clearSignals();

    // Replace in item2 camera1 with camera3.
    items[item2].resource.id = camera3->getId();
    layout->setItems(items);

    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_TRUE(watcher->hasResource(camera3->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet({layout}));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera3->getId())), LayoutSet({layout}));

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), camera1->getId());
    ASSERT_EQ(popNextLayout(), layout);

    ASSERT_EQ(popNextSignal(), QString("resourceRemoved"));
    ASSERT_EQ(popNextResourceId(), camera1->getId());

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), camera3->getId());
    ASSERT_EQ(popNextLayout(), layout);

    ASSERT_EQ(popNextSignal(), QString("resourceAdded"));
    ASSERT_EQ(popNextResourceId(), camera3->getId());

    ASSERT_FALSE(hasSignals());

    // Replace in item1 camera2 with camera3.
    items[item1].resource.id = camera3->getId();
    layout->setItems(items);

    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_TRUE(watcher->hasResource(camera3->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera3->getId())), LayoutSet({layout}));

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), camera2->getId());
    ASSERT_EQ(popNextLayout(), layout);

    ASSERT_EQ(popNextSignal(), QString("resourceRemoved"));
    ASSERT_EQ(popNextResourceId(), camera2->getId());

    ASSERT_FALSE(hasSignals());

    // Replace in item2 camera3 with null.
    items[item2].resource.id = nx::Uuid{};
    layout->setItems(items);

    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_TRUE(watcher->hasResource(camera3->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera3->getId())), LayoutSet({layout}));

    ASSERT_FALSE(hasSignals());

    // Replace in item1 camera3 with null.
    items[item1].resource.id = nx::Uuid{};
    layout->setItems(items);

    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_FALSE(watcher->hasResource(camera3->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera3->getId())), LayoutSet());

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), camera3->getId());
    ASSERT_EQ(popNextLayout(), layout);

    ASSERT_EQ(popNextSignal(), QString("resourceRemoved"));
    ASSERT_EQ(popNextResourceId(), camera3->getId());

    ASSERT_FALSE(hasSignals());

    // Replace in item1 null with camera1.
    items[item1].resource.id = camera1->getId();
    layout->setItems(items);

    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_FALSE(watcher->hasResource(camera3->getId()));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera1->getId())), LayoutSet({layout}));
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera2->getId())), LayoutSet());
    ASSERT_EQ(toSet(watcher->resourceLayouts(camera3->getId())), LayoutSet());

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), camera1->getId());
    ASSERT_EQ(popNextLayout(), layout);

    ASSERT_EQ(popNextSignal(), QString("resourceAdded"));
    ASSERT_EQ(popNextResourceId(), camera1->getId());

    ASSERT_FALSE(hasSignals());
}

} // namespace test
} // namespace nx::vms::common
