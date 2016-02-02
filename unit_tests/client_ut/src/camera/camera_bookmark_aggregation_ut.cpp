#include <gtest/gtest.h>

#include <camera/camera_bookmark_aggregation.h>

namespace
{
    QnCameraBookmark createValidBookmark() {
        QnCameraBookmark result;
        result.guid = QnUuid::createUuid();
        result.cameraId = "testcamera";
        result.durationMs = 60 * 1000;
        Q_ASSERT_X(result.isValid(), Q_FUNC_INFO, "This function must create valid bookmarks");

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

    list.addBookmark(bookmark);
    ASSERT_EQ(list.bookmarkList().size(), 1);
}

/** Test if invalid bookmark is not added. */
TEST( QnCameraBookmarkAggregationTest, addInvalid )
{
    QnCameraBookmarkAggregation list;

    QnCameraBookmark bookmark;
    ASSERT_FALSE(bookmark.isValid());

    list.addBookmark(bookmark);

    ASSERT_EQ(list.bookmarkList().size(), 0);
}