#include <random>

#include <gtest/gtest.h>
#include <api/test_api_requests.h>
#include <nx/network/http/custom_headers.h>

#include <nx/fusion/serialization/json.h>
#include <nx/fusion/model_functions.h>
#include <api/model/api_ioport_data.h>
#include <nx/network/http/multipart_content_parser.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <common/common_module.h>
#include <core/resource_management/status_dictionary.h>
#include <plugins/resource/test_camera/testcamera_resource.h>
#include <api/global_settings.h>

namespace {
    static const std::chrono::seconds kMaxTestTime(500);
    static const QString kTestCamPhysicalId("testCam15");
}

namespace nx {
namespace test {

class IoMonitorParser: public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    IoMonitorParser(const QnIOStateDataList& data): m_data(data) {}

    bool isEof() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_data.empty();
    }

protected:
    virtual bool processData(const QnByteArrayConstRef& data) override
    {

        bool ok = false;
        QnIOStateDataList receivedDataList = QJson::deserialized<QnIOStateDataList>(data, QnIOStateDataList(), &ok);
        if (!ok)
            return false;

        for (const auto& receivedData: receivedDataList)
        {
            QnMutexLocker lock(&m_mutex);

            QnIOStateData expectData;
            if (!m_data.empty())
            {
                expectData = m_data.front();
                m_data.erase(m_data.begin());
            }
            if (expectData != receivedData)
                return false;
        }

        return true;
    }

private:
    mutable QnMutex m_mutex;
    QnIOStateDataList m_data;
};

static QnIOStateDataList genTestData()
{
    QnIOStateDataList result;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> stateGen(0, 1);

    for (int i = 0; i < 1000; ++i)
    {
        QnIOStateData data(
            lit("A%1").arg(i), //< port
            bool(stateGen(gen)), //< state
            i); //< time
        result.push_back(data);
    }

    return result;
}

static void prepareDbTestData(const MediaServerLauncher* const launcher)
{
    ec2::ApiCameraData cameraData;
    auto resTypeId = qnResTypePool->getResourceTypeId(
        QnTestCameraResource::kManufacturer,
        QnTestCameraResource::kModel);
    auto resTypePtr = qnResTypePool->getResourceType(resTypeId);
    ASSERT_FALSE(resTypePtr.isNull());
    cameraData.typeId = resTypePtr->getId();
    cameraData.parentId = launcher->commonModule()->moduleGUID();
    cameraData.vendor = QnTestCameraResource::kManufacturer;
    cameraData.physicalId = kTestCamPhysicalId;
    cameraData.id = ec2::ApiCameraData::physicalIdToId(cameraData.physicalId);
    NX_TEST_API_POST(launcher, lit("/ec2/saveCamera"), cameraData);
}

TEST(IoServerMonitorTest, main)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());
    // Prevent opening stream from testCamera and moving camera to offline state
    launcher.commonModule()->globalSettings()->setAutoUpdateThumbnailsEnabled(false);

    prepareDbTestData(&launcher);
    auto testData = genTestData();

    auto httpClient = nx_http::AsyncHttpClient::create();
    auto httpClientGuard = makeScopeGuard([&httpClient]() { httpClient->pleaseStopSync(); });

    httpClient->setUserName("admin");
    httpClient->setUserPassword("admin");

    nx::utils::Url url = launcher.apiUrl();
    url.setPath("/api/iomonitor");
    QUrlQuery query;
    query.addQueryItem(Qn::PHYSICAL_ID_URL_QUERY_ITEM, kTestCamPhysicalId);
    url.setQuery(query);

    auto contentParser = std::make_shared<nx_http::MultipartContentParser>();
    auto ioParser = std::make_shared<IoMonitorParser>(testData);
    contentParser->setNextFilter(ioParser);

    std::promise<void> allDataProcessed;
    std::promise<void> ioServerStarted;

    QObject::connect(
        httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        [contentParser, &ioServerStarted](nx_http::AsyncHttpClientPtr httpClient)
        {
            ASSERT_EQ(nx_http::StatusCode::ok, httpClient->response()->statusLine.statusCode);
            ASSERT_TRUE(contentParser->setContentType(httpClient->contentType()));
            ioServerStarted.set_value();
        });

    QObject::connect(
        httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
        [contentParser, ioParser, &allDataProcessed](nx_http::AsyncHttpClientPtr httpClient)
        {
            const nx_http::BufferType& msgBodyBuf = httpClient->fetchMessageBodyBuffer();
            ASSERT_TRUE(contentParser->processData(msgBodyBuf));
            if (ioParser->isEof())
                allDataProcessed.set_value();
        });

    QObject::connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        [&allDataProcessed](nx_http::AsyncHttpClientPtr /*httpClient*/)
        {
            allDataProcessed.set_value();
        });

    auto camera = launcher.commonModule()->resourcePool()->getResourceByUniqueId<QnSecurityCamResource>(kTestCamPhysicalId);
    ASSERT_TRUE(camera);
    launcher.commonModule()->statusDictionary()->setValue(camera->getId(), Qn::Online);
    httpClient->doGet(url);

    ASSERT_EQ(
        std::future_status::ready,
        ioServerStarted.get_future().wait_for(kMaxTestTime));

    for (const auto& data: testData)
    {
        emit camera->cameraInput(
            camera->toSharedPointer(),
            data.id,
            data.isActive,
            data.timestamp);
    }

    ASSERT_EQ(
        std::future_status::ready,
        allDataProcessed.get_future().wait_for(kMaxTestTime));


    ASSERT_TRUE(ioParser->isEof());
}

} // namespace test
} // namespace nx
