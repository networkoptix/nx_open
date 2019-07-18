#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <api/test_api_requests.h>
#include <core/resource/camera_bookmark.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/network/url/url_builder.h>

using namespace nx::test;
using namespace std::chrono;

namespace nx::vms::server::api::test {

static const QString kLogicalId = "1";

class LogicalIdTest: public ::testing::Test
{
public:
    static void SetUpTestCase()
    {
        mediaServerLauncher.reset(new MediaServerLauncher());
        ASSERT_TRUE(mediaServerLauncher->start());

        // Add camera

        nx::vms::api::CameraDataList cameras;
        nx::vms::api::CameraData cameraData;
        cameraData.parentId = QnUuid::createUuid();
        cameraData.typeId = qnResTypePool->getResourceTypeByName("Camera")->getId();
        cameraData.vendor = "test vendor";
        cameraData.physicalId = lit("Some physicalId");
        cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId);
        cameras.push_back(cameraData);

        cameraData.physicalId = lit("Some physicalId 2");
        cameraData.id = nx::vms::api::CameraData::physicalIdToId(cameraData.physicalId);
        cameras.push_back(cameraData);

        auto connection = mediaServerLauncher->commonModule()->ec2Connection();
        auto cameraManager = connection->getCameraManager(Qn::kSystemAccess);
        auto dbError = cameraManager->addCamerasSync(cameras);
        ASSERT_EQ(ec2::ErrorCode::ok, dbError);

        nx::vms::api::CameraAttributesData userAttributeData;
        nx::vms::api::CameraAttributesDataList attributeList;
        userAttributeData.cameraId = cameraData.id;
        userAttributeData.logicalId = kLogicalId;
        attributeList.push_back(userAttributeData);

        cameraManager->saveUserAttributesSync(attributeList);
        ASSERT_EQ(ec2::ErrorCode::ok, dbError);

        // Add bookmarks
        auto serverDb = mediaServerLauncher->serverModule()->serverDb();
        QnCameraBookmark bookmark;
        bookmark.cameraId = cameraData.id;
        bookmark.startTimeMs = 1ms;
        bookmark.durationMs = 100ms;
        bookmark.name = "bookmark 1";
        bookmark.guid = QnUuid::createUuid();
        bookmark.tags << "tag1" << "tag2";
        serverDb->addBookmark(bookmark);

        bookmark.cameraId = QnUuid::createUuid();
        bookmark.name = "bookmark 2";
        serverDb->addBookmark(bookmark);

        std::vector<QnUuid> cameraHistoryData;
        cameraHistoryData.push_back(cameraData.id);
        const auto serverId = mediaServerLauncher->commonModule()->moduleGUID();
        ASSERT_EQ(ec2::ErrorCode::ok,
            cameraManager->setServerFootageDataSync(serverId, cameraHistoryData));
    }

    void checkFileDownload(const QString& urlPattern)
    {
        auto httpClient = std::make_unique<nx::network::http::HttpClient>();
        auto url = nx::network::url::Builder(mediaServerLauncher->apiUrl())
            .setUserName("admin")
            .setPassword("admin")
            .setPath(urlPattern.arg(kLogicalId));

        auto result = httpClient->doGet(url);
        ASSERT_TRUE(httpClient->response() != nullptr);
        auto code = httpClient->response()->statusLine.statusCode;
        ASSERT_EQ(nx::network::http::StatusCode::ok, code);

        url.setPath(urlPattern.arg("Invalid id"));
        result = httpClient->doGet(url);
        ASSERT_TRUE(httpClient->response() != nullptr);
        code = httpClient->response()->statusLine.statusCode;
        ASSERT_EQ(nx::network::http::StatusCode::notFound, code);
    }

    static void TearDownTestCase()
    {
        mediaServerLauncher.reset();
    }

public:
    static std::unique_ptr<MediaServerLauncher> mediaServerLauncher;
};

std::unique_ptr<MediaServerLauncher> LogicalIdTest::mediaServerLauncher;

TEST_F(LogicalIdTest, cameraList)
{
    nx::vms::api::CameraDataList cameras;
    NX_TEST_API_GET(mediaServerLauncher.get(), lit("/ec2/getCameras?id=%1").arg(kLogicalId), &cameras);
    ASSERT_EQ(1, cameras.size());
}

TEST_F(LogicalIdTest, cameraListEx)
{
    nx::vms::api::CameraDataExList cameras;
    NX_TEST_API_GET(mediaServerLauncher.get(), lit("/ec2/getCamerasEx?id=%1").arg(kLogicalId), &cameras);
    ASSERT_EQ(1, cameras.size());
    ASSERT_EQ(cameras[0].logicalId, kLogicalId);
}

TEST_F(LogicalIdTest, bookmarks)
{
    QnCameraBookmarkList bookmarks;
    NX_TEST_API_GET(mediaServerLauncher.get(), lit("/ec2/bookmarks?cameraId=%1").arg(kLogicalId), &bookmarks);
    ASSERT_EQ(1, bookmarks.size());
    ASSERT_EQ("bookmark 1", bookmarks[0].name);
}

TEST_F(LogicalIdTest, rtsp)
{
    QnRtspClient rtspClient{QnRtspClient::Config()};
    QAuthenticator auth;
    auth.setUser("admin");
    auth.setPassword("admin");
    rtspClient.setAuth(auth, nx::network::http::header::AuthScheme::digest);
    rtspClient.setPlayNowModeAllowed(true);
    rtspClient.setTCPTimeout(10s);
    auto url = nx::network::url::Builder(mediaServerLauncher->apiUrl())
        .setScheme("rtsp").setPath(kLogicalId);
    rtspClient.open(url);
    ASSERT_TRUE(rtspClient.play(0, 1000, 1.0));

    rtspClient.stop();
    url.setPath("missingCamera");
    rtspClient.open(url);
    ASSERT_FALSE(rtspClient.play(0, 1000, 1.0));
}

TEST_F(LogicalIdTest, hls)
{
    checkFileDownload("/hls/%1?pos=0&duration=10000");
}

TEST_F(LogicalIdTest, download)
{
    checkFileDownload("/media/%1.webm?pos=0&duration=10000");
}

} // nx::vms::server::api::test
