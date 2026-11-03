// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <QtGui/QStandardItemModel>

#include <client/client_globals.h>
#include <core/resource/user_resource.h>
#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/desktop/event_search/utils/event_preview_cameras.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/resource/camera_resource_stub.h>

namespace nx::vms::client::desktop::test {

using namespace std::chrono;
using namespace nx::vms::api;

namespace {

static constexpr microseconds kLive{0};
static constexpr microseconds kArchive = 1h;

} // namespace

class EventPreviewCamerasTest: public ContextBasedTest
{
protected:
    virtual void SetUp() override
    {
        ContextBasedTest::SetUp();

        liveCamera = addCamera();
        archiveCamera = addCamera();
        hiddenCamera = addCamera();
    }

    virtual void TearDown() override
    {
        systemContext()->accessController()->setUser({});
        ContextBasedTest::TearDown();
    }

    /** User with live access to one camera and archive access to another. */
    void loginWithPerCameraAccess()
    {
        const auto user = addUser(NoGroup,
            kTestUserName,
            UserType::local,
            GlobalPermission::none,
            {
                {liveCamera->getId(), AccessRight::view},
                {archiveCamera->getId(), AccessRight::view | AccessRight::viewArchive},
            });

        systemContext()->accessController()->setUser(user);
    }

    QnVirtualCameraResourcePtr liveCamera;
    QnVirtualCameraResourcePtr archiveCamera;
    QnVirtualCameraResourcePtr hiddenCamera;
};

TEST_F(EventPreviewCamerasTest, previewAccessDependsOnPreviewTime)
{
    loginWithPerCameraAccess();

    EXPECT_TRUE(isPreviewAccessible(liveCamera, kLive));
    EXPECT_FALSE(isPreviewAccessible(liveCamera, kArchive));

    EXPECT_TRUE(isPreviewAccessible(archiveCamera, kLive));
    EXPECT_TRUE(isPreviewAccessible(archiveCamera, kArchive));

    EXPECT_FALSE(isPreviewAccessible(hiddenCamera, kLive));
    EXPECT_FALSE(isPreviewAccessible(hiddenCamera, kArchive));
}

TEST_F(EventPreviewCamerasTest, fakeCameraIsNeverAccessible)
{
    loginWithPerCameraAccess();

    archiveCamera->addFlags(Qn::ResourceFlag::fake);
    EXPECT_FALSE(isPreviewAccessible(archiveCamera, kLive));
}

TEST_F(EventPreviewCamerasTest, crossSystemCameraIsAlwaysAccessible)
{
    loginWithPerCameraAccess();

    // Cross-system cameras are not in the local pool.
    const auto crossSystemCamera = createCamera();
    crossSystemCamera->addFlags(Qn::ResourceFlag::cross_system);
    EXPECT_TRUE(isPreviewAccessible(crossSystemCamera, kArchive));
}

TEST_F(EventPreviewCamerasTest, previewCamerasAreFilteredWithTileCameraFirst)
{
    loginWithPerCameraAccess();

    const QnVirtualCameraResourceList cameras{hiddenCamera, liveCamera, archiveCamera};

    EXPECT_EQ(previewCameras(cameras, archiveCamera, kLive),
        QnVirtualCameraResourceList({archiveCamera, liveCamera}));

    EXPECT_EQ(previewCameras(cameras, archiveCamera, kArchive),
        QnVirtualCameraResourceList({archiveCamera}));
}

TEST_F(EventPreviewCamerasTest, previewCamerasDontNeedAccessibleTileCamera)
{
    loginWithPerCameraAccess();

    const QnVirtualCameraResourceList cameras{liveCamera, archiveCamera};

    EXPECT_EQ(previewCameras(cameras, liveCamera, kArchive),
        QnVirtualCameraResourceList({archiveCamera}));
    EXPECT_EQ(previewCameras(cameras, {}, kLive), cameras);
}

TEST_F(EventPreviewCamerasTest, previewCountNeedsTileCameraFirst)
{
    const QnVirtualCameraResourceList cameras{liveCamera, archiveCamera};

    EXPECT_EQ(previewCount(cameras, liveCamera), 2);
    EXPECT_EQ(previewCount(cameras, archiveCamera), 0);
    EXPECT_EQ(previewCount(cameras, {}), 0);
    EXPECT_EQ(previewCount({}, liveCamera), 0);
}

TEST_F(EventPreviewCamerasTest, tooltipSkipsFullFrameTileSnapshot)
{
    const QnVirtualCameraResourceList cameras{liveCamera, archiveCamera, hiddenCamera};

    EXPECT_EQ(tooltipPreviewCameras(cameras, liveCamera, TileSnapshot::fullFrame),
        QnVirtualCameraResourceList({archiveCamera, hiddenCamera}));
}

TEST_F(EventPreviewCamerasTest, tooltipKeepsPartialTileSnapshot)
{
    const QnVirtualCameraResourceList cameras{liveCamera, archiveCamera};

    EXPECT_EQ(tooltipPreviewCameras(cameras, liveCamera, TileSnapshot::partial), cameras);
}

TEST_F(EventPreviewCamerasTest, tooltipKeepsTileCameraWithoutTileSnapshot)
{
    const QnVirtualCameraResourceList cameras{liveCamera, archiveCamera};

    EXPECT_EQ(tooltipPreviewCameras(cameras, liveCamera, TileSnapshot::none), cameras);
    EXPECT_EQ(tooltipPreviewCameras({liveCamera}, liveCamera, TileSnapshot::none),
        QnVirtualCameraResourceList({liveCamera}));
}

TEST_F(EventPreviewCamerasTest, tooltipKeepsAllIfTileCameraIsNotFirst)
{
    const QnVirtualCameraResourceList cameras{archiveCamera, hiddenCamera};

    EXPECT_EQ(tooltipPreviewCameras(cameras, liveCamera, TileSnapshot::fullFrame), cameras);
    EXPECT_EQ(tooltipPreviewCameras(cameras, {}, TileSnapshot::fullFrame), cameras);
    EXPECT_EQ(tooltipPreviewCameras({archiveCamera}, {}, TileSnapshot::fullFrame),
        QnVirtualCameraResourceList({archiveCamera}));
}

TEST_F(EventPreviewCamerasTest, tooltipHasNoSnapshotsForSingleTileCamera)
{
    EXPECT_TRUE(tooltipPreviewCameras({liveCamera}, liveCamera, TileSnapshot::fullFrame).empty());
    EXPECT_TRUE(tooltipPreviewCameras({liveCamera}, liveCamera, TileSnapshot::partial).empty());
    EXPECT_TRUE(tooltipPreviewCameras({}, liveCamera, TileSnapshot::fullFrame).empty());
}

TEST_F(EventPreviewCamerasTest, tileSnapshotKind)
{
    QStandardItemModel model;
    const auto addItem = [&model](int role, const QVariant& value)
    {
        auto item = new QStandardItem();
        if (role >= 0)
            item->setData(value, role);
        model.appendRow(item);
        return item->index();
    };

    EXPECT_EQ(tileSnapshot(addItem(-1, {})), TileSnapshot::fullFrame);
    EXPECT_EQ(tileSnapshot(addItem(core::ItemZoomRectRole, QRectF(0.1, 0.1, 0.5, 0.5))),
        TileSnapshot::partial);
    EXPECT_EQ(tileSnapshot(
                  addItem(core::ObjectTrackIdRole, QVariant::fromValue(nx::Uuid::createUuid()))),
        TileSnapshot::partial);
    EXPECT_EQ(tileSnapshot(addItem(Qn::ForcePreviewLoaderRole, true)), TileSnapshot::none);
    EXPECT_EQ(tileSnapshot(addItem(Qn::ShowVideoPreviewRole, true)), TileSnapshot::none);
}

TEST_F(EventPreviewCamerasTest, previewCameraIsFirstAccessibleOne)
{
    loginWithPerCameraAccess();

    const QnVirtualCameraResourceList cameras{hiddenCamera, liveCamera, archiveCamera};

    EXPECT_EQ(selectPreviewCamera(cameras, kLive), liveCamera);
    EXPECT_EQ(selectPreviewCamera(cameras, kArchive), archiveCamera);
}

TEST_F(EventPreviewCamerasTest, noPreviewCameraIfNoneIsVisible)
{
    loginWithPerCameraAccess();

    EXPECT_FALSE(selectPreviewCamera({hiddenCamera, liveCamera}, kArchive));
    EXPECT_FALSE(selectPreviewCamera({}, kLive));
}

} // namespace nx::vms::client::desktop::test
