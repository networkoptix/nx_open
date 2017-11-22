#include <memory>

#include <gtest/gtest.h>

#include <nx/network/aio/basic_pollable.h>

#include <analytics/detected_objects_storage/analytics_events_storage.h>
#include <api/mediaserver_client.h>
#include <test_support/analytics/storage/analytics_storage_types.h>
#include <test_support/mediaserver_launcher.h>

namespace test {

using namespace nx::analytics::storage;

namespace {

struct LookupRequestData
{
    Filter filter;
    ResultCode resultCode = ResultCode::ok;
    LookupResult responseData;
};

class EventsStorageMock:
    public AbstractEventsStorage
{
public:
    EventsStorageMock(nx::utils::SyncQueue<LookupRequestData>* lookupRequestQueue):
        m_lookupRequestQueue(lookupRequestQueue)
    {
    }

    virtual ~EventsStorageMock() override
    {
        m_asyncCaller.pleaseStopSync();
    }

    virtual bool initialize() override
    {
        return true;
    }

    virtual void save(
        nx::common::metadata::ConstDetectionMetadataPacketPtr /*packet*/,
        StoreCompletionHandler completionHandler) override
    {
        m_asyncCaller.post(
            [completionHandler = std::move(completionHandler)]
            {
                completionHandler(ResultCode::ok);
            });
    }

    virtual void createLookupCursor(
        Filter /*filter*/,
        CreateCursorCompletionHandler completionHandler) override
    {
        m_asyncCaller.post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(ResultCode::error, nullptr);
            });
    }

    virtual void lookup(
        Filter filter,
        LookupCompletionHandler completionHandler) override
    {
        m_asyncCaller.post(
            [this, filter = std::move(filter),
                completionHandler = std::move(completionHandler)]() mutable
            {
                LookupRequestData lookupRequestData;
                lookupRequestData.filter = std::move(filter);
                lookupRequestData.resultCode = ResultCode::ok;
                lookupRequestData.responseData = generateRandomLookupResult();
                m_lookupRequestQueue->push(lookupRequestData);
                completionHandler(lookupRequestData.resultCode, lookupRequestData.responseData);
            });
    }

    virtual void markDataAsDeprecated(
        QnUuid /*deviceId*/,
        qint64 /*oldestNeededDataTimestamp*/) override
    {
    }

private:
    nx::utils::SyncQueue<LookupRequestData>* m_lookupRequestQueue;
    nx::network::aio::BasicPollable m_asyncCaller;

    LookupResult generateRandomLookupResult()
    {
        LookupResult result;
        // TODO
        return result;
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

class AnalyticsEventsStorageHttpApi:
    public ::testing::Test
{
public:
    AnalyticsEventsStorageHttpApi()
    {
        using namespace std::placeholders;

        m_factoryBak = EventsStorageFactory::instance().setCustomFunc(
            std::bind(&AnalyticsEventsStorageHttpApi::createEventsStorageMock, this, _1));
    }

    ~AnalyticsEventsStorageHttpApi()
    {
        EventsStorageFactory::instance().setCustomFunc(std::move(m_factoryBak));
    }

protected:
    void whenIssueLookup()
    {
        using namespace std::placeholders;

        m_filter = nx::analytics::storage::test::generateRandomFilter();

        m_mediaserverClient->ec2AnalyticsLookupDetectedObjects(
            m_filter,
            std::bind(&AnalyticsEventsStorageHttpApi::saveLookupResult, this, _1, _2));
    }

    void thenLookupIsMadeToStorage()
    {
        m_prevLookupRequest = m_lookupRequests.pop();
        m_prevLookupResponse = m_lookupResponses.pop();
    }

    void andCorrectResultCodeReturned()
    {
        ASSERT_EQ(m_prevLookupRequest.resultCode, m_prevLookupResponse.resultCode);
    }

    void andFilterIsPassedCorrectly()
    {
        ASSERT_EQ(m_filter, m_prevLookupRequest.filter);
    }

    void andLookupResultIsDeliveredCorrectly()
    {
        ASSERT_EQ(m_prevLookupRequest.responseData, m_prevLookupResponse.data);
    }

private:
    struct LookupResponseData
    {
        ResultCode resultCode = ResultCode::ok;
        LookupResult data;
    };

    std::unique_ptr<MediaServerLauncher> m_mediaServerLauncher;
    EventsStorageFactory::Function m_factoryBak;
    std::unique_ptr<MediaServerClient> m_mediaserverClient;
    Filter m_filter;
    nx::utils::SyncQueue<LookupRequestData> m_lookupRequests;
    LookupRequestData m_prevLookupRequest;
    nx::utils::SyncQueue<LookupResponseData> m_lookupResponses;
    LookupResponseData m_prevLookupResponse;

    virtual void SetUp() override
    {
        m_mediaServerLauncher = std::make_unique<MediaServerLauncher>();
        ASSERT_TRUE(m_mediaServerLauncher->start());

        // TODO: #ak Remove it from here when mediaserver does it
        // from QnMain::run, not from application's main thread.
        m_mediaServerLauncher->commonModule()->messageProcessor()->init(
            m_mediaServerLauncher->commonModule()->ec2Connection());

        m_mediaserverClient = prepareMediaServerClient();
    }

    std::unique_ptr<AbstractEventsStorage> createEventsStorageMock(
        const Settings& /*settings*/)
    {
        return std::make_unique<EventsStorageMock>(&m_lookupRequests);
    }

    std::unique_ptr<MediaServerClient> prepareMediaServerClient()
    {
        auto mediaServerClient = std::make_unique<MediaServerClient>(
            nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
            .setEndpoint(m_mediaServerLauncher->endpoint()));
        mediaServerClient->setUserCredentials(
            nx_http::Credentials("admin", nx_http::PasswordAuthToken("admin")));
        return mediaServerClient;
    }

    void saveLookupResult(ec2::ErrorCode resultCode, LookupResult result)
    {
        LookupResponseData lookupResponseData;
        lookupResponseData.resultCode =
            resultCode == ec2::ErrorCode::ok ? ResultCode::ok : ResultCode::error;
        lookupResponseData.data = std::move(result);
        m_lookupResponses.push(std::move(lookupResponseData));
    }
};

TEST_F(AnalyticsEventsStorageHttpApi, analyticsLookupDetectedObjects_correctly_returns_data)
{
    whenIssueLookup();

    thenLookupIsMadeToStorage();
    andCorrectResultCodeReturned();
    andFilterIsPassedCorrectly();
    andLookupResultIsDeliveredCorrectly();
}

} // namespace test
