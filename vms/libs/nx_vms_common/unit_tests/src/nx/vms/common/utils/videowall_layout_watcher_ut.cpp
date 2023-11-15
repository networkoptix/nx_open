// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <set>

#include <core/resource/layout_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/test_support/test_context.h>
#include <nx/vms/common/utils/videowall_layout_watcher.h>

namespace nx::vms::common {
namespace test {

class VideowallLayoutWatcherTest: public ContextBasedTest
{
public:
    virtual void SetUp() override
    {
        ContextBasedTest::SetUp();
        watcher = VideowallLayoutWatcher::instance(resourcePool());

        QObject::connect(watcher.get(), &VideowallLayoutWatcher::videowallLayoutsAdded,
            [this](const QnVideoWallResourcePtr& videowall, const QVector<QnUuid>& layoutIds)
            {
                lastSignals.push_back("videowallLayoutsAdded");
                lastLayoutIds.append(layoutIds);
                lastVideowalls.push_back(videowall);
            });

        QObject::connect(watcher.get(), &VideowallLayoutWatcher::videowallLayoutsRemoved,
            [this](const QnVideoWallResourcePtr& videowall, const QVector<QnUuid>& layoutIds)
            {
                lastSignals.push_back("videowallLayoutsRemoved");
                lastLayoutIds.append(layoutIds);
                lastVideowalls.push_back(videowall);
            });

        QObject::connect(watcher.get(), &VideowallLayoutWatcher::videowallAdded,
            [this](const QnVideoWallResourcePtr& videowall)
            {
                lastSignals.push_back("videowallAdded");
                lastVideowalls.push_back(videowall);
            });

        QObject::connect(watcher.get(), &VideowallLayoutWatcher::videowallRemoved,
            [this](const QnVideoWallResourcePtr& videowall)
            {
                lastSignals.push_back("videowallRemoved");
                lastVideowalls.push_back(videowall);
            });
    }

    virtual void TearDown() override
    {
        lastSignals.clear();
        lastVideowalls.clear();
        lastLayoutIds.clear();
        watcher.reset();
        ContextBasedTest::TearDown();
    }

    QString popNextSignal()
    {
        return lastSignals.empty() ? QString() : lastSignals.takeFirst();
    }

    QnUuid popNextLayoutId()
    {
        return lastLayoutIds.empty() ? QnUuid() : lastLayoutIds.takeFirst();
    }

    QnVideoWallResourcePtr popNextVideowall()
    {
        return lastVideowalls.empty() ? QnVideoWallResourcePtr() : lastVideowalls.takeFirst();
    }

    QnUuidSet toSet(const QnCounterHash<QnUuid>& ids)
    {
        return QnUuidSet(ids.keyBegin(), ids.keyEnd());
    }

public:
    std::shared_ptr<VideowallLayoutWatcher> watcher;

private:
    QStringList lastSignals;
    QList<QnUuid> lastLayoutIds;
    QList<QnVideoWallResourcePtr> lastVideowalls;
};

TEST_F(VideowallLayoutWatcherTest, instanceSharing)
{
    const auto otherPointer = VideowallLayoutWatcher::instance(resourcePool());
    ASSERT_EQ(watcher, otherPointer);

    std::unique_ptr<QnResourcePool> otherPool(new QnResourcePool(systemContext()));
    const auto otherInstance = VideowallLayoutWatcher::instance(otherPool.get());
    ASSERT_NE(watcher, otherInstance);
}

TEST_F(VideowallLayoutWatcherTest, addRemoveVideowallLayout)
{
    const auto videowall = addVideoWall();
    ASSERT_EQ(popNextSignal(), QString("videowallAdded"));
    ASSERT_EQ(popNextVideowall(), videowall);

    const auto layout1 = addLayout();
    const auto item1 = addVideoWallItem(videowall, layout1);
    ASSERT_EQ(popNextSignal(), QString("videowallLayoutsAdded"));
    ASSERT_EQ(popNextLayoutId(), layout1->getId());
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout1), videowall);
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)), QnUuidSet({layout1->getId()}));

    const auto layout2 = addLayout();
    const auto item2 = addVideoWallItem(videowall, layout2);
    ASSERT_EQ(popNextSignal(), QString("videowallLayoutsAdded"));
    ASSERT_EQ(popNextLayoutId(), layout2->getId());
    ASSERT_EQ(popNextVideowall(), videowall);

    const auto item3 = addVideoWallItem(videowall, layout2);
    ASSERT_EQ(popNextSignal(), QString());
    ASSERT_EQ(watcher->layoutVideowall(layout2), videowall);
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)),
        QnUuidSet({layout1->getId(), layout2->getId()}));

    videowall->items()->removeItem(item1);
    ASSERT_EQ(popNextSignal(), QString("videowallLayoutsRemoved"));
    ASSERT_EQ(popNextLayoutId(), layout1->getId());
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout1), QnVideoWallResourcePtr());
    ASSERT_EQ(watcher->layoutVideowall(layout2), videowall);
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)), QnUuidSet({layout2->getId()}));

    videowall->items()->removeItem(item2);
    ASSERT_EQ(popNextSignal(), QString());
    ASSERT_EQ(watcher->layoutVideowall(layout2), videowall);
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)), QnUuidSet({layout2->getId()}));

    videowall->items()->removeItem(item3);
    ASSERT_EQ(popNextSignal(), QString("videowallLayoutsRemoved"));
    ASSERT_EQ(popNextLayoutId(), layout2->getId());
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout2), QnVideoWallResourcePtr());
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)), QnUuidSet());
}

TEST_F(VideowallLayoutWatcherTest, addRemoveVideowall)
{
    const auto videowall = createVideoWall();
    const auto layout1 = addLayoutForVideoWall(videowall);
    const auto layout2 = addLayoutForVideoWall(videowall);
    addVideoWallItem(videowall, layout2);

    // Videowall is not in the pool yet.
    ASSERT_EQ(popNextSignal(), QString());
    ASSERT_EQ(watcher->layoutVideowall(layout1), QnVideoWallResourcePtr());
    ASSERT_EQ(watcher->layoutVideowall(layout2), QnVideoWallResourcePtr());
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)), QnUuidSet());

    resourcePool()->addResource(videowall);
    ASSERT_EQ(popNextSignal(), QString("videowallAdded"));
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout1), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout2), videowall);
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)),
        QnUuidSet({layout1->getId(), layout2->getId()}));

    resourcePool()->removeResource(videowall);
    ASSERT_EQ(popNextSignal(), QString("videowallRemoved"));
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout1), QnVideoWallResourcePtr());
    ASSERT_EQ(watcher->layoutVideowall(layout2), QnVideoWallResourcePtr());
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)), QnUuidSet());
}

TEST_F(VideowallLayoutWatcherTest, videowallMatrices)
{
    const auto videowall = addVideoWall();
    ASSERT_EQ(popNextSignal(), QString("videowallAdded"));
    ASSERT_EQ(popNextVideowall(), videowall);

    const auto layout1 = addLayout();
    const auto layout2 = addLayout();
    const auto item1 = addVideoWallItem(videowall, layout1);
    ASSERT_EQ(popNextSignal(), QString("videowallLayoutsAdded"));
    ASSERT_EQ(popNextLayoutId(), layout1->getId());
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout1), videowall);
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)), QnUuidSet({layout1->getId()}));

    const auto item2 = addVideoWallItem(videowall, layout2);
    ASSERT_EQ(popNextSignal(), QString("videowallLayoutsAdded"));
    ASSERT_EQ(popNextLayoutId(), layout2->getId());
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout2), videowall);
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)),
        QnUuidSet({layout1->getId(), layout2->getId()}));

    QnVideoWallMatrix matrix1;
    matrix1.uuid = QnUuid::createUuid();
    matrix1.layoutByItem[item1] = layout1->getId();
    matrix1.layoutByItem[item2] = layout2->getId();
    videowall->matrices()->addItem(matrix1);

    QnVideoWallMatrix matrix2;
    matrix2.uuid = QnUuid::createUuid();
    matrix2.layoutByItem[item1] = layout1->getId();
    matrix2.layoutByItem[item2] = QnUuid{};
    videowall->matrices()->addItem(matrix2);

    // Saved matrices cause no change in videowall layout resolution.
    ASSERT_EQ(popNextSignal(), QString());

    ASSERT_TRUE(changeVideoWallItem(videowall, item1, {}));
    ASSERT_TRUE(changeVideoWallItem(videowall, item2, {}));

    // Clearing videowall items also causes no change, as layouts reside in saved matrices.
    ASSERT_EQ(popNextSignal(), QString());
    ASSERT_EQ(watcher->layoutVideowall(layout1), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout2), videowall);
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)),
        QnUuidSet({layout1->getId(), layout2->getId()}));

    // Remove layout2 from matrix1; layout2 will be removed from the videowall resolution.
    matrix1.layoutByItem[item2] = QnUuid{};
    videowall->matrices()->addOrUpdateItem(matrix1);
    ASSERT_EQ(popNextSignal(), QString("videowallLayoutsRemoved"));
    ASSERT_EQ(popNextLayoutId(), layout2->getId());
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout2), QnVideoWallResourcePtr{});
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)), QnUuidSet({layout1->getId()}));

    // Remove item1 from the videowall; layout1 will be removed from the videowall resolution.
    videowall->items()->removeItem(item1);
    ASSERT_EQ(popNextSignal(), QString("videowallLayoutsRemoved"));
    ASSERT_EQ(popNextLayoutId(), layout1->getId());
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout1), QnVideoWallResourcePtr{});
    ASSERT_TRUE(watcher->videowallLayouts(videowall).empty());

    // Add layout1 to matrix1; layout1 will be added to the videowall resolution.
    matrix1.layoutByItem[item2] = layout1->getId();
    videowall->matrices()->addOrUpdateItem(matrix1);
    ASSERT_EQ(popNextSignal(), QString("videowallLayoutsAdded"));
    ASSERT_EQ(popNextLayoutId(), layout1->getId());
    ASSERT_EQ(popNextVideowall(), videowall);
    ASSERT_EQ(watcher->layoutVideowall(layout1), videowall);
    ASSERT_EQ(toSet(watcher->videowallLayouts(videowall)), QnUuidSet({layout1->getId()}));
}

} // namespace test
} // namespace nx::vms::common
