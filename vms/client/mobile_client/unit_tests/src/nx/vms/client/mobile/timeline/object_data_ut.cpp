// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>

#include <gtest/gtest.h>

#include <analytics/db/analytics_db_types.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/client/mobile/timeline/analytics_data_helper.h>
#include <nx/vms/client/mobile/timeline/bookmark_data_helper.h>
#include <nx/vms/client/mobile/timeline/motion_data_helper.h>

#include "../test_support/test_context.h"

namespace nx::vms::client::mobile::timeline::test {

using namespace std::chrono;

class ObjectDataTest: public mobile::test::ApplicationContextBasedTest
{
public:
    common::CameraBookmark makeBookmark(const QString& name) const
    {
        common::CameraBookmark bookmark;
        bookmark.guid = nx::Uuid::createUuid();
        bookmark.cameraId = camera->getId();
        bookmark.name = name;
        bookmark.description = "Description";
        bookmark.startTimeMs = 100000ms;
        bookmark.durationMs = 5s;
        bookmark.tags = {"tag"};
        return bookmark;
    }

    virtual void SetUp() override { camera = addCamera(); }

    virtual void TearDown() override
    {
        camera.reset();
        mobile::test::ApplicationContextBasedTest::TearDown();
    }

    QnVirtualCameraResourcePtr camera;
};

TEST_F(ObjectDataTest, bookmarkData)
{
    const auto bookmark = makeBookmark("Name");
    const auto data = BookmarkDataHelper::makeObjectData(bookmark, camera);

    ASSERT_EQ(data.id, bookmark.guid);
    ASSERT_EQ(data.startTimeMs, 100000);
    ASSERT_EQ(data.durationMs, 5000);
    ASSERT_EQ(data.title, "Name");
    ASSERT_EQ(data.description, "Description");
    ASSERT_EQ(data.tags, QStringList{"tag"});
    ASSERT_FALSE(data.imagePath.isEmpty());
    ASSERT_FALSE(data.shared);
    ASSERT_EQ(data.source.metaType(), QMetaType::fromType<common::CameraBookmark>());
}

TEST_F(ObjectDataTest, unnamedBookmarkData)
{
    const auto data = BookmarkDataHelper::makeObjectData(makeBookmark(""), camera);
    ASSERT_EQ(data.title, BookmarkDataHelper::tr("Bookmark"));
}

TEST_F(ObjectDataTest, bookmarkToBookmark)
{
    const auto bookmark = makeBookmark("Name");
    const auto result = toBookmark(BookmarkDataHelper::makeObjectData(bookmark, camera), camera);

    ASSERT_EQ(result.guid, bookmark.guid);
    ASSERT_EQ(result.name, bookmark.name);
    ASSERT_EQ(result.startTimeMs, bookmark.startTimeMs);
    ASSERT_EQ(result.durationMs, bookmark.durationMs);
}

TEST_F(ObjectDataTest, analyticsData)
{
    nx::analytics::db::ObjectTrack track;
    track.id = nx::Uuid::createUuid();
    track.deviceId = camera->getId();
    track.objectTypeId = "test.car";
    track.firstAppearanceTimeUs = microseconds(100000ms).count();
    track.lastAppearanceTimeUs = microseconds(105000ms).count();
    track.attributes.emplace_back("Color", "Red");

    const auto data = AnalyticsDataHelper::makeObjectData(track, camera);

    ASSERT_EQ(data.id, track.id);
    ASSERT_EQ(data.startTimeMs, 100000);
    ASSERT_EQ(data.durationMs, 5000);
    ASSERT_FALSE(data.imagePath.isEmpty());
    ASSERT_FALSE(data.iconPath.isEmpty());
    ASSERT_EQ(data.attributes.size(), 1);
    ASSERT_EQ(data.attributes.front().id, "Color");
    ASSERT_EQ(data.source.metaType(), QMetaType::fromType<nx::analytics::db::ObjectTrack>());
}

TEST_F(ObjectDataTest, motionData)
{
    const QnTimePeriod motion(100000, 4000);
    const auto data = MotionDataHelper::makeObjectData(motion, camera);

    ASSERT_EQ(data.startTimeMs, 100000);
    ASSERT_EQ(data.durationMs, 4000);
    ASSERT_EQ(data.title, MotionDataHelper::tr("Motion detected"));
    ASSERT_EQ(data.iconPath, MotionDataHelper::iconPath());
    ASSERT_TRUE(data.imagePath.contains("time=102000")); //< The middle of the motion.
    ASSERT_EQ(data.source.value<QnTimePeriod>(), motion);
}

} // namespace nx::vms::client::mobile::timeline::test
