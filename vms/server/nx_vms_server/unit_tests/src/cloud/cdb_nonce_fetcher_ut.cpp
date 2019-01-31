#include <gtest/gtest.h>

#include <nx/cloud/db/api/cloud_nonce.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/time.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/utils/uuid.h>

#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <nx/vms/cloud_integration/cdb_nonce_fetcher.h>

namespace nx {
namespace vms {
namespace vms::server {
namespace test {

namespace {

//-------------------------------------------------------------------------------------------------

class CloudData
{
public:
    void setFetchedNonceEventReceiver(
        nx::utils::SyncQueue<std::string>* fetchedNonceEventReceiver)
    {
        QnMutexLocker lock(&m_mutex);
        m_fetchedNonceEventReceiver = fetchedNonceEventReceiver;
    }

    std::string addNonce(
        const std::string& systemId,
        std::chrono::seconds expirationTime)
    {
        QnMutexLocker lock(&m_mutex);

        m_nonce = nx::cloud::db::api::NonceData();
        m_nonce->nonce = nx::cloud::db::api::generateCloudNonceBase(systemId);
        m_nonce->validPeriod = expirationTime;

        return m_nonce->nonce;
    }

    boost::optional<nx::cloud::db::api::NonceData> getNonce() const
    {
        QnMutexLocker lock(&m_mutex);
        if (m_nonce && m_fetchedNonceEventReceiver)
            m_fetchedNonceEventReceiver->push(m_nonce->nonce);
        return m_nonce;
    }

private:
    boost::optional<nx::cloud::db::api::NonceData> m_nonce;
    nx::utils::SyncQueue<std::string>* m_fetchedNonceEventReceiver = nullptr;
    mutable QnMutex m_mutex;
};

//-------------------------------------------------------------------------------------------------

class DummyAuthProvider:
    public nx::cloud::db::api::AuthProvider
{
public:
    DummyAuthProvider(CloudData* cloudData):
        m_cloudData(cloudData)
    {
    }

    ~DummyAuthProvider()
    {
        m_aioObject.pleaseStopSync();
    }

    virtual void getCdbNonce(
        std::function<void(nx::cloud::db::api::ResultCode, nx::cloud::db::api::NonceData)> completionHandler) override
    {
        const auto nonce = m_cloudData->getNonce();
        if (nonce)
        {
            m_aioObject.post(std::bind(completionHandler,
                nx::cloud::db::api::ResultCode::ok, *nonce));
        }
        else
        {
            m_aioObject.post(std::bind(completionHandler,
                nx::cloud::db::api::ResultCode::forbidden, nx::cloud::db::api::NonceData()));
        }
    }

    virtual void getCdbNonce(
        const std::string& /*systemId*/,
        std::function<void(nx::cloud::db::api::ResultCode, nx::cloud::db::api::NonceData)> completionHandler) override
    {
        m_aioObject.post(std::bind(completionHandler, nx::cloud::db::api::ResultCode::ok, nx::cloud::db::api::NonceData()));
    }

    virtual void getAuthenticationResponse(
        const nx::cloud::db::api::AuthRequest& /*authRequest*/,
        std::function<void(nx::cloud::db::api::ResultCode, nx::cloud::db::api::AuthResponse)> completionHandler) override
    {
        m_aioObject.post(std::bind(completionHandler,
            nx::cloud::db::api::ResultCode::notImplemented, nx::cloud::db::api::AuthResponse()));
    }

    void bindToAioThread(network::aio::AbstractAioThread* aioThread)
    {
        m_aioObject.bindToAioThread(aioThread);
    }

private:
    nx::network::aio::BasicPollable m_aioObject;
    CloudData* m_cloudData = nullptr;
};

//-------------------------------------------------------------------------------------------------

class DummyCloudConnection:
    public nx::cloud::db::api::Connection
{
public:
    DummyCloudConnection(CloudData* cloudData):
        m_authProvider(cloudData)
    {
        m_authProvider.bindToAioThread(m_aioObject.getAioThread());
    }

    ~DummyCloudConnection()
    {
        m_aioObject.pleaseStopSync();
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        m_aioObject.bindToAioThread(aioThread);
    }

    virtual void setCredentials(
        const std::string& /*login*/,
        const std::string& /*password*/) override
    {
    }

    virtual void setProxyCredentials(
        const std::string& /*login*/,
        const std::string& /*password*/) override
    {
    }

    virtual void setProxyVia(
        const std::string& /*proxyHost*/,
        std::uint16_t /*proxyPort*/) override
    {
    }

    virtual void setRequestTimeout(std::chrono::milliseconds) override {}

    virtual std::chrono::milliseconds requestTimeout() const override
    {
        return std::chrono::milliseconds::zero();
    }

    virtual nx::cloud::db::api::AccountManager* accountManager() override { return nullptr; }

    virtual nx::cloud::db::api::SystemManager* systemManager() override { return nullptr; }

    virtual nx::cloud::db::api::AuthProvider* authProvider() override { return &m_authProvider; }

    virtual nx::cloud::db::api::MaintenanceManager* maintenanceManager() override { return nullptr; }

    virtual void ping(
        std::function<void(nx::cloud::db::api::ResultCode, nx::cloud::db::api::ModuleInfo)> completionHandler) override
    {
        m_aioObject.post(std::bind(completionHandler, nx::cloud::db::api::ResultCode::ok, nx::cloud::db::api::ModuleInfo()));
    }

private:
    nx::network::aio::BasicPollable m_aioObject;
    DummyAuthProvider m_authProvider;
};

//-------------------------------------------------------------------------------------------------

class DummyCloudConnectionManager:
    public nx::vms::cloud_integration::AbstractCloudConnectionManager
{
public:
    DummyCloudConnectionManager(
        CloudData* cloudData,
        std::string cloudSystemId)
        :
        m_cloudData(cloudData),
        m_cloudSystemId(cloudSystemId)
    {
    }

    virtual std::optional<nx::hpm::api::SystemCredentials> getSystemCredentials() const override
    {
        if (m_cloudSystemId.empty())
            return std::nullopt;

        nx::hpm::api::SystemCredentials systemCredentials;
        systemCredentials.systemId = m_cloudSystemId.c_str();
        systemCredentials.key = nx::utils::generateRandomName(7);
        return systemCredentials;
    }

    virtual bool boundToCloud() const override
    {
        return !m_cloudSystemId.empty();
    }

    virtual std::unique_ptr<nx::cloud::db::api::Connection> getCloudConnection(
        const QString& /*cloudSystemId*/,
        const QString& /*cloudAuthKey*/) const override
    {
        return std::make_unique<DummyCloudConnection>(m_cloudData);
    }

    virtual std::unique_ptr<nx::cloud::db::api::Connection> getCloudConnection() override
    {
        return std::make_unique<DummyCloudConnection>(m_cloudData);
    }

    virtual void processCloudErrorCode(nx::cloud::db::api::ResultCode /*resultCode*/) override
    {
    }

    void setCloudSystemId(std::string cloudSystemId)
    {
        m_cloudSystemId = cloudSystemId;
    }

private:
    CloudData* m_cloudData;
    std::string m_cloudSystemId;
};

//-------------------------------------------------------------------------------------------------

class DummyCloudUserInfoPool:
    public nx::vms::cloud_integration::AbstractCloudUserInfoPool
{
public:
    DummyCloudUserInfoPool(std::string cloudSystemId):
        m_cloudSystemId(cloudSystemId)
    {
    }

    virtual boost::optional<nx::Buffer> newestMostCommonNonce() const override
    {
        return m_nonce;
    }

    virtual void clear() override
    {
        m_nonce.reset();
    }

    void emulateUserPresence()
    {
        m_nonce = nx::cloud::db::api::generateCloudNonceBase(m_cloudSystemId).c_str();
    }

private:
    boost::optional<nx::Buffer> m_nonce;
    std::string m_cloudSystemId;

    virtual void userInfoChanged(
        const nx::Buffer& /*userName*/,
        const nx::cloud::db::api::AuthInfo& /*authInfo*/) override
    {
    }

    virtual void userInfoRemoved(const nx::Buffer& /*userName*/) override
    {
    }
};

//-------------------------------------------------------------------------------------------------

class DummyNonceProvider:
    public nx::vms::auth::AbstractNonceProvider
{
public:
    virtual QByteArray generateNonce() override
    {
        return QByteArray();
    }

    virtual bool isNonceValid(const QByteArray& /*nonce*/) const override
    {
        return false;
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

class CdbNonceFetcher:
    public ::testing::Test
{
public:
    CdbNonceFetcher():
        m_cloudSystemId(QnUuid::createUuid().toSimpleString().toStdString()),
        m_cloudConnectionManager(&m_cloudData, m_cloudSystemId),
        m_cloudUserInfoPool(m_cloudSystemId),
        m_cdbNonceFetcher(
            &m_cloudConnectionManager,
            &m_cloudUserInfoPool,
            &m_nonceProvider)
    {
        m_cloudData.setFetchedNonceEventReceiver(&m_fetchedNonceEventReceiver);
    }

protected:
    void givenCloudUser()
    {
        m_cloudUserInfoPool.emulateUserPresence();
    }

    void givenEmptyCloudSystemId()
    {
        m_cloudConnectionManager.setCloudSystemId(std::string());
    }

    void whenRequestNonce()
    {
        m_prevNonce = m_cdbNonceFetcher.generateNonce();
    }

    void thenNonceIsTakenFromUserAttributes()
    {
        ASSERT_TRUE(m_prevNonce.startsWith(*m_cloudUserInfoPool.newestMostCommonNonce()));
    }

    void thenNonceIsConsideredValid()
    {
        ASSERT_TRUE(m_cdbNonceFetcher.isNonceValid(m_prevNonce));
    }

    void cloudProvidesNonceValidForSomeTime()
    {
        m_latestCloudNonce = m_cloudData.addNonce(
            m_cloudSystemId,
            std::chrono::seconds(1));
    }

    void cloudProvidesNonceValidInfinitely()
    {
        m_latestCloudNonce = m_cloudData.addNonce(
            m_cloudSystemId,
            std::chrono::hours(1));
    }

    void waitUntilTheLatestCloudNonceIsProvided()
    {
        for (;;)
        {
            const auto nonce = m_cdbNonceFetcher.generateNonce();
            if (nonce.startsWith(m_latestCloudNonce.c_str()))
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void waitUntilTheLatestCloudNonceIsFetchedFromCloud()
    {
        while (m_fetchedNonceEventReceiver.pop() != m_latestCloudNonce)
        {
        }
    }

private:
    CloudData m_cloudData;
    std::string m_cloudSystemId;
    DummyCloudConnectionManager m_cloudConnectionManager;
    DummyCloudUserInfoPool m_cloudUserInfoPool;
    DummyNonceProvider m_nonceProvider;
    nx::vms::cloud_integration::CdbNonceFetcher m_cdbNonceFetcher;
    QByteArray m_prevNonce;
    std::string m_latestCloudNonce;
    nx::utils::SyncQueue<std::string> m_fetchedNonceEventReceiver;
};

TEST_F(CdbNonceFetcher, nonce_taken_from_cloud_user_attributes_is_accepted)
{
    givenCloudUser();

    whenRequestNonce();

    thenNonceIsTakenFromUserAttributes();
    thenNonceIsConsideredValid();
}

TEST_F(CdbNonceFetcher, generated_nonce_is_accepted_even_if_cloud_system_id_is_absent)
{
    givenCloudUser();
    givenEmptyCloudSystemId();

    whenRequestNonce();

    thenNonceIsConsideredValid();
}

TEST_F(CdbNonceFetcher, requests_cloud_nonce_initially)
{
    // Given no cloud users with offline login support.

    cloudProvidesNonceValidInfinitely();
    waitUntilTheLatestCloudNonceIsProvided();
}

TEST_F(CdbNonceFetcher, requests_cloud_nonce_periodically)
{
    // Given no cloud users with offline login support.

    cloudProvidesNonceValidForSomeTime();
    waitUntilTheLatestCloudNonceIsFetchedFromCloud();

    cloudProvidesNonceValidInfinitely();
    waitUntilTheLatestCloudNonceIsProvided();
}

} // namespace test
} // namespace vms::server
} // namespace vms
} // namespace nx
