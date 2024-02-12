// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <camera/camera_bookmark_aggregation.h>

#include <nx/utils/log/assert.h>

using namespace std::chrono;

namespace
{
    const milliseconds kDefaultBookmarkDuration = 60s;

    QnCameraBookmark createValidBookmark(milliseconds startTimeMs = 0ms)
    {
        QnCameraBookmark result;
        result.guid = nx::Uuid::createUuid();
        result.name = "Bookmark";
        result.cameraId = nx::Uuid::fromArbitraryData(QByteArray("testcamera"));
        result.startTimeMs = startTimeMs;
        result.durationMs = kDefaultBookmarkDuration;
        NX_ASSERT(result.isValid(), "This function must create valid bookmarks");

        return result;
    }

}

/** Initial test. Check if default list is empty. */
TEST( QnCameraBookmarkAggregationTest, init )
{
    QnCameraBookmarkAggregation list;
    ASSERT_EQ(list.bookmarkList().size(), 0);
}

/** Test if first bookmark is added correctly. */
TEST( QnCameraBookmarkAggregationTest, add )
{
    QnCameraBookmarkAggregation list;

    QnCameraBookmark bookmark(createValidBookmark());
    ASSERT_TRUE(bookmark.isValid());

    bool changed = list.addBookmark(bookmark);
    ASSERT_EQ(1, list.bookmarkList().size());
    ASSERT_TRUE(changed);
}

/** Test if null bookmark is not added. */
TEST( QnCameraBookmarkAggregationTest, addNull )
{
    QnCameraBookmarkAggregation list;

    QnCameraBookmark bookmark;
    ASSERT_TRUE(bookmark.isNull());

#ifdef _DEBUG
    ASSERT_DEATH(list.addBookmark(bookmark), "");
#else
    /* Following checks are actual in release mode. */
    bool changed = list.addBookmark(bookmark);
    ASSERT_FALSE(changed);
#endif

    ASSERT_EQ(0, list.bookmarkList().size());
}

/** Test if bookmark is not duplicated. */
TEST( QnCameraBookmarkAggregationTest, addSame )
{
    QnCameraBookmarkAggregation list;

    QnCameraBookmark bookmark(createValidBookmark());
    ASSERT_TRUE(bookmark.isValid());

    list.addBookmark(bookmark);

    bool changed = list.addBookmark(bookmark);

    ASSERT_EQ(1, list.bookmarkList().size());
    ASSERT_FALSE(changed);
}
