#include <QFile>
#include <vector>

#include <gtest/gtest.h>
#include <server/server_globals.h>
#include <test_support/utils.h>
#include <test_support/mediaserver_launcher.h>
#include <api/app_server_connection.h>

#include "common/common_module.h"
#include <core/resource/camera_bookmark.h>
#include <nx/utils/elapsed_timer.h>
#include <database/server_db.h>

namespace nx {
namespace mediaserver_core {
namespace bookmarks {
namespace test {

using namespace std::chrono;
using namespace std::literals::chrono_literals;

class BookmarksDatabaseTest: public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        mediaServerLauncher.reset(new MediaServerLauncher());
    }

    static void TearDownTestCase()
    {
        mediaServerLauncher.reset();
    }

    virtual void SetUp() override
    {
    }
public:
    static std::unique_ptr<MediaServerLauncher> mediaServerLauncher;
};

std::unique_ptr<MediaServerLauncher> BookmarksDatabaseTest::mediaServerLauncher;

TEST_F(BookmarksDatabaseTest, DISABLED_speedTest)
{
    mediaServerLauncher->addSetting("eventsDBFilePath", "c:/test_database");
    ASSERT_TRUE(mediaServerLauncher->start());

    QList<QnUuid> cameras;
    cameras << QnUuid("6FD1F239-CEBC-81BF-C2D4-59789E2CEF04");

    QnCameraBookmarkSearchFilter filter;
    filter.endTimeMs = milliseconds(QDateTime::currentMSecsSinceEpoch());
    filter.startTimeMs = filter.endTimeMs - 24h;
    nx::utils::ElapsedTimer timer;
    timer.restart();
    QnCameraBookmarkList result;
    qnServerDb->getBookmarks(cameras, filter, result);
    auto elapsedMs = timer.elapsedMs();
    NX_DEBUG(this, lm("Loading bookmarks time: %1, count: %2").args(elapsedMs, result.size()));
}

TEST_F(BookmarksDatabaseTest, selectTest)
{
    ASSERT_TRUE(mediaServerLauncher->start());

    QnUuid cameraId1("6FD1F239-CEBC-81BF-C2D4-59789E2CEF04");
    QnUuid cameraId2("6FD1F239-CEBC-81BF-C2D4-59789E2CEF05");

    const auto endTimeMs = milliseconds(QDateTime::currentMSecsSinceEpoch());
    const auto periodMs = 24h * 30;
    const auto startTimeMs = endTimeMs - periodMs;

    QnCameraBookmark bookmark;
    bookmark.cameraId = cameraId1;
    bookmark.startTimeMs = startTimeMs + periodMs / 2;
    bookmark.durationMs = 100ms;
    bookmark.name = "bookmarkMid";
    bookmark.guid = QnUuid::createUuid();
    bookmark.tags << "tag1" << "tag2";
    qnServerDb->addBookmark(bookmark);

    bookmark.startTimeMs = startTimeMs - bookmark.durationMs / 2;
    bookmark.name = "bookmarkLeft";
    bookmark.tags.clear();
    bookmark.guid = QnUuid::createUuid();
    qnServerDb->addBookmark(bookmark);

    bookmark.startTimeMs = endTimeMs + 1ms;
    bookmark.name = "bookmarRight";
    bookmark.guid = QnUuid::createUuid();
    qnServerDb->addBookmark(bookmark);

    bookmark.startTimeMs = startTimeMs - bookmark.durationMs / 2;
    bookmark.name = "bookmarkMid 2";
    bookmark.cameraId = cameraId2;
    bookmark.guid = QnUuid::createUuid();
    qnServerDb->addBookmark(bookmark);

    QList<QnUuid> cameras;
    cameras << cameraId1;

    QnCameraBookmarkSearchFilter filter;
    filter.endTimeMs = endTimeMs;
    filter.startTimeMs = startTimeMs;
    filter.limit = 100;

    QnCameraBookmarkList result;
    qnServerDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ("bookmarkLeft", result[0].name);
    ASSERT_EQ("bookmarkMid", result[1].name);
    ASSERT_EQ(2, result[1].tags.size());
    ASSERT_TRUE(result[1].tags.contains("tag1"));
    ASSERT_TRUE(result[1].tags.contains("tag2"));

    result.clear();
    filter.text = "tag2";
    qnServerDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(2, result[0].tags.size());
    ASSERT_TRUE(result[0].tags.contains("tag1"));
    ASSERT_TRUE(result[0].tags.contains("tag2"));

    result.clear();
    filter.text = "tag3";
    qnServerDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(0, result.size());
}

} // namespace test
} // namespace bookmarks
} // namespace mediaserver_core
} // namespace nx
