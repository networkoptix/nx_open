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
namespace vms::server {
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
    cameras << QUuid("{3645c7ee-ca91-e579-e753-1d85af1fd08c}");
    cameras << QUuid("{8e42995b-74d1-2fb6-8ca6-1c298a6a70f7}");
    cameras << QUuid("{95f1cf54-0b91-4afd-966c-2e9db69c0ce1}");
    cameras << QUuid("{97bad347-f07f-8457-cc86-76d1810bdd1a}");
    cameras << QUuid("{37304dee-5ea6-62c0-a70f-b8e9b560fddc}");
    cameras << QUuid("{9102f0d4-f69a-d703-39e4-7c46352e0cb9}");
    cameras << QUuid("{38c5e727-b304-d188-0733-8619d09baae5}");
    cameras << QUuid("{d3c72398-df57-e80f-49f4-d8f1ab9432f5}");
    cameras << QUuid("{f033913a-7eaa-4aec-b5da-37da3aa86f29}");
    cameras << QUuid("{b79d4acc-aea1-302d-468c-e4519dcd5d2e}");
    cameras << QUuid("{02e450a9-f75a-d00b-0031-47798c2c8b81}");
    cameras << QUuid("{ac7b2f8a-1abd-9693-6d72-bec6100ae546}");
    cameras << QUuid("{a7178593-98ad-eb04-4b85-e3cc353e245a}");
    cameras << QUuid("{ed93120e-0f50-3cdf-39c8-dd52a640688c}");
    cameras << QUuid("{785d91dd-471d-b9b1-dc42-9829baa9be71}");
    cameras << QUuid("{9e270512-ee84-f563-5117-4352959af346}");
    cameras << QUuid("{e3e9a385-7fe0-3ba5-5482-a86cde7faf48}");
    cameras << QUuid("{5dbc8d8c-ee79-b125-2b41-31015f17cda4}");
    cameras << QUuid("{91b275d3-e524-17e4-ceec-dc621d4f2d2f}");
    cameras << QUuid("{2df4280e-ea1a-d8e8-1fdf-a4e91d4f4e81}");
    cameras << QUuid("{b6a279d3-0244-8d69-aa30-aca5e05cbfc4}");
    cameras << QUuid("{b4372200-7ce4-7277-577a-58a199ca28a0}");
    cameras << QUuid("{522007fa-1279-c117-2131-2071e3f28afd}");
    cameras << QUuid("{c2917c3a-0e37-8a99-8613-ab098807d466}");
    cameras << QUuid("{bbf6e391-beb5-828a-64f7-86677fb65430}");
    cameras << QUuid("{dd756ef8-db2b-da45-19ec-23930854bc08}");
    cameras << QUuid("{6fd1f239-cebc-81bf-c2d4-59789e2cef04}");
    cameras << QUuid("{3ce36e62-7416-4f42-0339-0adca55a9091}");
    cameras << QUuid("{7cde947f-e163-7dda-20a6-2e2344005251}");
    cameras << QUuid("{e222af0b-9617-d3c4-8c79-f3fc52307491}");
    cameras << QUuid("{d5a0e41b-def7-606b-f0e0-136128982907}");
    cameras << QUuid("{627b12a0-32bc-5585-a418-fabbfa2a2274}");
    cameras << QUuid("{aeef9f34-90d1-3ac3-c419-1486ae960335}");
    cameras << QUuid("{0ce6db71-2a66-94c4-2774-85f79ff85a1f}");
    cameras << QUuid("{6d92a881-1bc2-5104-dc6b-894c0fa662c7}");
    cameras << QUuid("{0c4fb8b6-655b-2e55-9e34-32f088273f6a}");

    QnCameraBookmarkSearchFilter filter;
    filter.endTimeMs = 1530824400000ms;
    filter.startTimeMs = 1527454800000ms;
    filter.limit = 1000;
    nx::utils::ElapsedTimer timer;
    timer.restart();
    QnCameraBookmarkList result;
    mediaServerLauncher->serverModule()->serverDb()->getBookmarks(cameras, filter, result);
    auto elapsedMs = timer.elapsedMs();
    NX_DEBUG(this, lm("Loading bookmarks time: %1, count: %2").args(elapsedMs, result.size()));
}

TEST_F(BookmarksDatabaseTest, selectTest)
{
    ASSERT_TRUE(mediaServerLauncher->start());
    auto serverDb = mediaServerLauncher->serverModule()->serverDb();

    static const QnUuid kCameraId1("6FD1F239-CEBC-81BF-C2D4-59789E2CEF04");
    static const QnUuid kCameraId2("6FD1F239-CEBC-81BF-C2D4-59789E2CEF05");

    mediaServerLauncher->serverModule()->serverDb()->deleteAllBookmarksForCamera(kCameraId1);
    mediaServerLauncher->serverModule()->serverDb()->deleteAllBookmarksForCamera(kCameraId2);

    const milliseconds startTimeMs = 500ms;
    const milliseconds periodMs = 1000ms;
    const milliseconds endTimeMs = startTimeMs + periodMs;

    QnCameraBookmark bookmark;
    bookmark.cameraId = kCameraId1;
    bookmark.startTimeMs = startTimeMs + periodMs / 2;
    bookmark.durationMs = 100ms;
    bookmark.name = "bookmarkMid";
    bookmark.guid = QnUuid::createUuid();
    bookmark.tags << "tag1" << "tag2";
    serverDb->addBookmark(bookmark);

    bookmark.startTimeMs = startTimeMs - bookmark.durationMs / 2;
    bookmark.name = "bookmarkLeft";
    bookmark.tags.clear();
    bookmark.guid = QnUuid::createUuid();
    serverDb->addBookmark(bookmark);

    bookmark.startTimeMs = endTimeMs + 1ms;
    bookmark.name = "bookmarRight";
    bookmark.guid = QnUuid::createUuid();
    serverDb->addBookmark(bookmark);

    bookmark.startTimeMs = startTimeMs - bookmark.durationMs / 2;
    bookmark.name = "bookmarkMid 2";
    bookmark.cameraId = kCameraId2;
    bookmark.guid = QnUuid::createUuid();
    serverDb->addBookmark(bookmark);

    QList<QnUuid> cameras;
    cameras << kCameraId1;

    QnCameraBookmarkSearchFilter filter;
    filter.endTimeMs = endTimeMs;
    filter.startTimeMs = startTimeMs;
    filter.limit = 100;

    QnCameraBookmarkList result;
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ("bookmarkLeft", result[0].name);
    ASSERT_EQ("bookmarkMid", result[1].name);
    ASSERT_EQ(2, result[1].tags.size());
    ASSERT_TRUE(result[1].tags.contains("tag1"));
    ASSERT_TRUE(result[1].tags.contains("tag2"));

    result.clear();
    filter.sparsing.minVisibleLengthMs = 101ms;
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(0, result.size());

    result.clear();
    filter.sparsing.minVisibleLengthMs = 100ms;
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(2, result.size());

    result.clear();
    filter.text = "tag2";
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(2, result[0].tags.size());
    ASSERT_TRUE(result[0].tags.contains("tag1"));
    ASSERT_TRUE(result[0].tags.contains("tag2"));

    result.clear();
    filter.text = "tag3";
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(0, result.size());
}

TEST_F(BookmarksDatabaseTest, rangeTest)
{
    ASSERT_TRUE(mediaServerLauncher->start());
    auto serverDb = mediaServerLauncher->serverModule()->serverDb();

    static const QnUuid kCameraId1("6FD1F239-CEBC-81BF-C2D4-59789E2CEF04");
    serverDb->deleteAllBookmarksForCamera(kCameraId1);

    for (int i = 0; i < 100 * 1000; i += 1000)
    {
        QnCameraBookmark bookmark;
        bookmark.cameraId = kCameraId1;
        bookmark.startTimeMs = milliseconds(i);
        bookmark.durationMs = 500ms;
        bookmark.name = QString::number(i);
        bookmark.guid = QnUuid::createUuid();
        ASSERT_TRUE(serverDb->addBookmark(bookmark));
    }

    QList<QnUuid> cameras;
    cameras << kCameraId1;
    QnCameraBookmarkSearchFilter filter;
    filter.startTimeMs = 1000ms;
    filter.endTimeMs = 1600ms;
    filter.limit = 100;

    QnCameraBookmarkList result;
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(1000ms, result[0].startTimeMs);

    filter.startTimeMs = 1000ms;
    filter.endTimeMs = 1600ms;
    filter.limit = 100;

    result.clear();
    filter.startTimeMs = 500ms;
    filter.endTimeMs = 1600ms;
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(0ms, result[0].startTimeMs);
    ASSERT_EQ(1000ms, result[1].startTimeMs);

    result.clear();
    filter.startTimeMs = 1100ms;
    filter.endTimeMs = 2000ms;
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(1000ms, result[0].startTimeMs);
    ASSERT_EQ(2000ms, result[1].startTimeMs);

    result.clear();
    filter.startTimeMs = 999ms;
    filter.endTimeMs = 1001ms;
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(1000ms, result[0].startTimeMs);

    result.clear();
    filter.startTimeMs = 0ms;
    filter.endTimeMs = 1000ms;
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(0ms, result[0].startTimeMs);
    ASSERT_EQ(1000ms, result[1].startTimeMs);

    result.clear();
    filter.orderBy.order = Qt::DescendingOrder;
    serverDb->getBookmarks(cameras, filter, result);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(0ms, result[1].startTimeMs);
    ASSERT_EQ(1000ms, result[0].startTimeMs);
}

TEST_F(BookmarksDatabaseTest, tagSearchTest)
{
    ASSERT_TRUE(mediaServerLauncher->start());

    // Testing bookmark search for tags
    static const QnUuid kCameraId1("6FD1F239-CEBC-81BF-C2D4-59789E2CEF04");
    mediaServerLauncher->serverModule()->serverDb()->deleteAllBookmarksForCamera(kCameraId1);

    // Keeping bookmarks for comparison with DB variant
    QnCameraBookmarkList bookmarks;

    for (int i = 0; i < 4; ++i)
    {
        QnCameraBookmark bookmark;
        bookmark.cameraId = kCameraId1;
        bookmark.startTimeMs = milliseconds(i);
        bookmark.durationMs = 500ms;
        bookmark.name = QString::number(i);
        bookmark.guid = QnUuid::createUuid();
        auto tag = QString("btag%1").arg(i);
        bookmark.tags = {tag};
        ASSERT_TRUE(mediaServerLauncher->serverModule()->serverDb()->addBookmark(bookmark));
        bookmarks.append(bookmark);
    }

    QString testTag = *bookmarks[0].tags.begin();

    QList<QnUuid> cameras = {kCameraId1};

    QnCameraBookmarkSearchFilter filter;
    filter.limit = 100;
    filter.text = testTag;

    // Should get only 1 bookmark
    QnCameraBookmarkList result;
    mediaServerLauncher->serverModule()->serverDb()->getBookmarks(cameras, filter, result);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(1, result[0].guid == bookmarks[0].guid);
    result.clear();

    // Removing all the tags and trying to search again. We should not get
    // this bookmark from search
    bookmarks[0].tags = {};
    mediaServerLauncher->serverModule()->serverDb()->updateBookmark(bookmarks[0]);

    mediaServerLauncher->serverModule()->serverDb()->getBookmarks(cameras, filter, result);
    ASSERT_EQ(0, result.size());

    result.clear();
    filter.text = "btag";
    mediaServerLauncher->serverModule()->serverDb()->getBookmarks(cameras, filter, result);
    ASSERT_EQ(3, result.size());
}
} // namespace test
} // namespace bookmarks
} // namespace vms::server
} // namespace nx
