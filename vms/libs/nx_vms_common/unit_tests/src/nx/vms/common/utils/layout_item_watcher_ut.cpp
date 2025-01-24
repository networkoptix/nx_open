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

    int signalCount() const
    {
        return (int) lastSignals.size();
    }

    void clearSignals()
    {
        lastSignals.clear();
        lastLayouts.clear();
        lastResourceIds.clear();
    }

    using LayoutSet = QnLayoutResourceSet;
    using ResourceIds = std::multiset<nx::Uuid>;

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
    ASSERT_EQ(watcher->resourceLayouts(camera->getId()), LayoutSet());

    const auto item1 = addToLayout(layout1, camera);
    ASSERT_TRUE(watcher->hasResource(camera->getId()));
    ASSERT_EQ(watcher->resourceLayouts(camera->getId()), LayoutSet({layout1}));

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), camera->getId());
    ASSERT_EQ(popNextLayout(), layout1);

    ASSERT_EQ(popNextSignal(), QString("resourceAdded"));
    ASSERT_EQ(popNextResourceId(), camera->getId());

    const auto item2 = addToLayout(layout2, camera);
    ASSERT_TRUE(watcher->hasResource(camera->getId()));
    ASSERT_EQ(watcher->resourceLayouts(camera->getId()), LayoutSet({layout1, layout2}));

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), camera->getId());
    ASSERT_EQ(popNextLayout(), layout2);

    layout1->removeItem(item1);
    ASSERT_TRUE(watcher->hasResource(camera->getId()));
    ASSERT_EQ(watcher->resourceLayouts(camera->getId()), LayoutSet({layout2}));

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    ASSERT_EQ(popNextResourceId(), camera->getId());
    ASSERT_EQ(popNextLayout(), layout1);

    layout2->removeItem(item2);
    ASSERT_FALSE(watcher->hasResource(camera->getId()));
    ASSERT_EQ(watcher->resourceLayouts(camera->getId()), LayoutSet());

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
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet());

    watcher->addWatchedLayout(layout1);
    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet({layout1}));
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet({layout1}));

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
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet({layout1, layout2}));
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet({layout1, layout2}));

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), layout2_item1);
    ASSERT_EQ(popNextLayout(), layout2);

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), layout2_item2);
    ASSERT_EQ(popNextLayout(), layout2);

    watcher->removeWatchedLayout(layout1);
    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet({layout2}));
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet({layout2}));

    ResourceIds removedIds;
    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    removedIds.insert(popNextResourceId());
    ASSERT_EQ(popNextLayout(), layout1);
    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    removedIds.insert(popNextResourceId());
    ASSERT_EQ(popNextLayout(), layout1);
    ASSERT_EQ(removedIds, ResourceIds({layout1_item1, layout1_item2}));

    watcher->removeWatchedLayout(layout2);
    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet());

    ResourceIds removedFromWatcher;
    removedIds.clear();
    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    removedIds.insert(popNextResourceId());
    ASSERT_EQ(popNextLayout(), layout2);

    ASSERT_EQ(popNextSignal(), QString("resourceRemoved"));
    removedFromWatcher.insert(popNextResourceId());

    ASSERT_EQ(popNextSignal(), QString("removedFromLayout"));
    removedIds.insert(popNextResourceId());
    ASSERT_EQ(popNextLayout(), layout2);

    ASSERT_EQ(popNextSignal(), QString("resourceRemoved"));
    removedFromWatcher.insert(popNextResourceId());

    ASSERT_EQ(removedIds, ResourceIds({layout2_item1, layout2_item2}));
    ASSERT_EQ(removedIds, ResourceIds({layout2_item1, layout2_item2}));
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
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet({layout}));
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet({layout}));
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet());

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
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet({layout}));
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet({layout}));
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet());

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
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet({layout}));
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet({layout}));

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
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet({layout}));

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
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet({layout}));

    ASSERT_FALSE(hasSignals());

    // Replace in item1 camera3 with null.
    items[item1].resource.id = nx::Uuid{};
    layout->setItems(items);

    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_FALSE(watcher->hasResource(camera3->getId()));
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet());

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
    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet({layout}));
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet());

    ASSERT_EQ(popNextSignal(), QString("addedToLayout"));
    ASSERT_EQ(popNextResourceId(), camera1->getId());
    ASSERT_EQ(popNextLayout(), layout);

    ASSERT_EQ(popNextSignal(), QString("resourceAdded"));
    ASSERT_EQ(popNextResourceId(), camera1->getId());

    ASSERT_FALSE(hasSignals());
}

TEST_F(LayoutItemWatcherTest, layoutWatchedFromUpdate)
{
    const auto layout = createLayout();
    const auto camera1 = createCamera();
    const auto camera2 = createCamera();
    const auto camera3 = createCamera();
    const auto camera4 = createCamera();

    const auto item1 = addToLayout(layout, camera1);
    const auto item2 = addToLayout(layout, camera2);

    QObject::connect(layout.get(), &QnResource::parentIdChanged,
        [this, layout]() { watcher->addWatchedLayout(layout); });

    const auto otherLayout = createLayout();
    otherLayout->setIdUnsafe(layout->getId());
    otherLayout->update(layout);

    otherLayout->setParentId(nx::Uuid::createUuid());
    auto items = otherLayout->getItems();

    // Change camera1 to camera4:
    items[item1].resource.id = camera4->getId();
    // Remove camera2:
    items.remove(item2);
    // Add camera3:
    const auto item3 = nx::Uuid::createUuid();
    nx::vms::common::LayoutItemData itemData;
    itemData.uuid = item3;
    itemData.resource.id = camera3->getId();
    items.insert(item3, itemData);

    otherLayout->setItems(items);

    // Update source layout: parentId and items are changed.
    // During the update, `QnResource::parentIdChanged` signal is sent and in the connected handler
    // the layout is added to the watcher, already with the new, changed items.
    // Then `QnLayoutResource::itemAdded`/`itemRemoved` signals are sent from the same update
    // and the watcher should properly ignore them.
    layout->update(otherLayout);

    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_TRUE(watcher->hasResource(camera3->getId()));
    ASSERT_TRUE(watcher->hasResource(camera4->getId()));

    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet({layout}));
    ASSERT_EQ(watcher->resourceLayouts(camera4->getId()), LayoutSet({layout}));

    QHash<QString, ResourceIds> signalResources;
    QList<QnLayoutResourcePtr> signalLayouts;

    const int expectedSignalCount = 4;
    ASSERT_EQ(signalCount(), expectedSignalCount);
    for (int i = 0; i < expectedSignalCount; ++i)
        signalResources[popNextSignal()].insert(popNextResourceId());

    // No removals: layout is added to the watcher with already changed items.
    ASSERT_EQ(signalResources["resourceRemoved"], ResourceIds());
    ASSERT_EQ(signalResources["removedFromLayout"], ResourceIds());

    ASSERT_EQ(signalResources["resourceAdded"],
        ResourceIds({camera3->getId(), camera4->getId()}));

    ASSERT_EQ(signalResources["addedToLayout"],
        ResourceIds({camera3->getId(), camera4->getId()}));

    ASSERT_EQ(popNextLayout(), layout);
    ASSERT_EQ(popNextLayout(), layout);
}

TEST_F(LayoutItemWatcherTest, layoutWatchedFromUpdate_sharedResources)
{
    // The same as `layoutWatchedFromUpdate` test, but added resources are shared with another
    // watched layout.

    const auto layout = createLayout();
    const auto camera1 = createCamera();
    const auto camera2 = createCamera();
    const auto camera3 = createCamera();
    const auto camera4 = createCamera();

    const auto item1 = addToLayout(layout, camera1);
    const auto item2 = addToLayout(layout, camera2);

    QObject::connect(layout.get(), &QnResource::parentIdChanged,
        [this, layout]() { watcher->addWatchedLayout(layout); });

    // All 4 cameras are added to another watched layout.
    const auto otherWatchedLayout = createLayout();
    addToLayout(otherWatchedLayout, camera1);
    addToLayout(otherWatchedLayout, camera2);
    addToLayout(otherWatchedLayout, camera3);
    addToLayout(otherWatchedLayout, camera4);
    watcher->addWatchedLayout(otherWatchedLayout);

    clearSignals();

    const auto otherLayout = createLayout();
    otherLayout->setIdUnsafe(layout->getId());
    otherLayout->update(layout);

    otherLayout->setParentId(nx::Uuid::createUuid());
    auto items = otherLayout->getItems();

    // Change camera1 to camera4:
    items[item1].resource.id = camera4->getId();
    // Remove camera2:
    items.remove(item2);
    // Add camera3:
    const auto item3 = nx::Uuid::createUuid();
    nx::vms::common::LayoutItemData itemData;
    itemData.uuid = item3;
    itemData.resource.id = camera3->getId();
    items.insert(item3, itemData);

    otherLayout->setItems(items);

    // Update source layout: parentId and items are changed.
    // During the update, `QnResource::parentIdChanged` signal is sent and in the connected handler
    // the layout is added to the watcher, already with the new, changed items.
    // Then `QnLayoutResource::itemAdded`/`itemRemoved` signals are sent from the same update
    // and the watcher should properly ignore them.
    layout->update(otherLayout);

    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_TRUE(watcher->hasResource(camera3->getId()));
    ASSERT_TRUE(watcher->hasResource(camera4->getId()));

    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet({otherWatchedLayout}));
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet({otherWatchedLayout}));
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet({layout, otherWatchedLayout}));
    ASSERT_EQ(watcher->resourceLayouts(camera4->getId()), LayoutSet({layout, otherWatchedLayout}));

    QHash<QString, ResourceIds> signalResources;
    QList<QnLayoutResourcePtr> signalLayouts;

    const int expectedSignalCount = 2;
    ASSERT_EQ(signalCount(), expectedSignalCount);
    for (int i = 0; i < expectedSignalCount; ++i)
        signalResources[popNextSignal()].insert(popNextResourceId());

    // No removals: layout is added to the watcher with already changed items.
    ASSERT_EQ(signalResources["resourceRemoved"], ResourceIds());
    ASSERT_EQ(signalResources["removedFromLayout"], ResourceIds());

    // No added resources: they're already in `otherWatchedLayout`.
    ASSERT_EQ(signalResources["resourceAdded"], ResourceIds());

    ASSERT_EQ(signalResources["addedToLayout"],
        ResourceIds({camera3->getId(), camera4->getId()}));

    ASSERT_EQ(popNextLayout(), layout);
    ASSERT_EQ(popNextLayout(), layout);
}

TEST_F(LayoutItemWatcherTest, layoutUnwatchedFromUpdate)
{
    const auto layout = createLayout();
    const auto camera1 = createCamera();
    const auto camera2 = createCamera();
    const auto camera3 = createCamera();
    const auto camera4 = createCamera();

    const auto item1 = addToLayout(layout, camera1);
    const auto item2 = addToLayout(layout, camera2);

    watcher->addWatchedLayout(layout);
    clearSignals();

    QObject::connect(layout.get(), &QnResource::parentIdChanged,
        [this, layout]() { watcher->removeWatchedLayout(layout); });

    const auto otherLayout = createLayout();
    otherLayout->setIdUnsafe(layout->getId());
    otherLayout->update(layout);

    otherLayout->setParentId(nx::Uuid::createUuid());
    auto items = otherLayout->getItems();

    // Change camera1 to camera4:
    items[item1].resource.id = camera4->getId();
    // Remove camera2:
    items.remove(item2);
    // Add camera3:
    const auto item3 = nx::Uuid::createUuid();
    nx::vms::common::LayoutItemData itemData;
    itemData.uuid = item3;
    itemData.resource.id = camera3->getId();
    items.insert(item3, itemData);

    otherLayout->setItems(items);

    // Update source layout: parentId and items are changed.
    // During the update, `QnResource::parentIdChanged` signal is sent and in the connected handler
    // the layout is removed from the watcher, already with the new, changed items.
    // Then `QnLayoutResource::itemAdded`/`itemRemoved` signals are sent from the same update
    // and the watcher should properly ignore them.
    layout->update(otherLayout);

    ASSERT_FALSE(watcher->hasResource(camera1->getId()));
    ASSERT_FALSE(watcher->hasResource(camera2->getId()));
    ASSERT_FALSE(watcher->hasResource(camera3->getId()));
    ASSERT_FALSE(watcher->hasResource(camera4->getId()));

    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet());
    ASSERT_EQ(watcher->resourceLayouts(camera4->getId()), LayoutSet());

    QHash<QString, ResourceIds> signalResources;
    QList<QnLayoutResourcePtr> signalLayouts;

    const int expectedSignalCount = 4;
    ASSERT_EQ(signalCount(), expectedSignalCount);
    for (int i = 0; i < expectedSignalCount; ++i)
        signalResources[popNextSignal()].insert(popNextResourceId());

    // No additions: layout is removed from the watcher before item change signals are processed.
    ASSERT_EQ(signalResources["resourceAdded"], ResourceIds());
    ASSERT_EQ(signalResources["addedToLayout"], ResourceIds());

    ASSERT_EQ(signalResources["resourceRemoved"],
        ResourceIds({camera1->getId(), camera2->getId()}));

    ASSERT_EQ(signalResources["removedFromLayout"],
        ResourceIds({camera1->getId(), camera2->getId()}));

    ASSERT_EQ(popNextLayout(), layout);
    ASSERT_EQ(popNextLayout(), layout);
}

TEST_F(LayoutItemWatcherTest, layoutUnwatchedFromUpdate_sharedResources)
{
    // The same as `layoutUnwatchedFromUpdate` test, but added resources are shared with another
    // watched layout.

    const auto layout = createLayout();
    const auto camera1 = createCamera();
    const auto camera2 = createCamera();
    const auto camera3 = createCamera();
    const auto camera4 = createCamera();

    const auto item1 = addToLayout(layout, camera1);
    const auto item2 = addToLayout(layout, camera2);

    watcher->addWatchedLayout(layout);

    QObject::connect(layout.get(), &QnResource::parentIdChanged,
        [this, layout]() { watcher->removeWatchedLayout(layout); });

    // All 4 cameras are added to another watched layout.
    const auto otherWatchedLayout = createLayout();
    addToLayout(otherWatchedLayout, camera1);
    addToLayout(otherWatchedLayout, camera2);
    addToLayout(otherWatchedLayout, camera3);
    addToLayout(otherWatchedLayout, camera4);
    watcher->addWatchedLayout(otherWatchedLayout);
    clearSignals();

    const auto otherLayout = createLayout();
    otherLayout->setIdUnsafe(layout->getId());
    otherLayout->update(layout);

    otherLayout->setParentId(nx::Uuid::createUuid());
    auto items = otherLayout->getItems();

    // Change camera1 to camera4:
    items[item1].resource.id = camera4->getId();
    // Remove camera2:
    items.remove(item2);
    // Add camera3:
    const auto item3 = nx::Uuid::createUuid();
    nx::vms::common::LayoutItemData itemData;
    itemData.uuid = item3;
    itemData.resource.id = camera3->getId();
    items.insert(item3, itemData);

    otherLayout->setItems(items);

    // Update source layout: parentId and items are changed.
    // During the update, `QnResource::parentIdChanged` signal is sent and in the connected handler
    // the layout is removed from the watcher, already with the new, changed items.
    // Then `QnLayoutResource::itemAdded`/`itemRemoved` signals are sent from the same update
    // and the watcher should properly ignore them.
    layout->update(otherLayout);

    ASSERT_TRUE(watcher->hasResource(camera1->getId()));
    ASSERT_TRUE(watcher->hasResource(camera2->getId()));
    ASSERT_TRUE(watcher->hasResource(camera3->getId()));
    ASSERT_TRUE(watcher->hasResource(camera4->getId()));

    ASSERT_EQ(watcher->resourceLayouts(camera1->getId()), LayoutSet({otherWatchedLayout}));
    ASSERT_EQ(watcher->resourceLayouts(camera2->getId()), LayoutSet({otherWatchedLayout}));
    ASSERT_EQ(watcher->resourceLayouts(camera3->getId()), LayoutSet({otherWatchedLayout}));
    ASSERT_EQ(watcher->resourceLayouts(camera4->getId()), LayoutSet({otherWatchedLayout}));

    QHash<QString, ResourceIds> signalResources;
    QList<QnLayoutResourcePtr> signalLayouts;

    const int expectedSignalCount = 2;
    ASSERT_EQ(signalCount(), expectedSignalCount);
    for (int i = 0; i < expectedSignalCount; ++i)
        signalResources[popNextSignal()].insert(popNextResourceId());

    // No additions: layout is removed from the watcher before item change signals are processed.
    ASSERT_EQ(signalResources["resourceAdded"], ResourceIds());
    ASSERT_EQ(signalResources["addedToLayout"], ResourceIds());
    ASSERT_EQ(signalResources["resourceRemoved"], ResourceIds());

    ASSERT_EQ(signalResources["removedFromLayout"],
        ResourceIds({camera1->getId(), camera2->getId()}));

    ASSERT_EQ(popNextLayout(), layout);
    ASSERT_EQ(popNextLayout(), layout);
}

} // namespace test
} // namespace nx::vms::common
