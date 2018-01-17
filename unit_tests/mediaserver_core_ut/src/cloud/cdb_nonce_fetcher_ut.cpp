#include <gtest/gtest.h>

#include <nx/cloud/cdb/api/cloud_nonce.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/uuid.h>

#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <nx/vms/cloud_integration/cdb_nonce_fetcher.h>

namespace nx {
namespace vms {
namespace mediaserver {
namespace test {

namespace {

class DummyCloudConnectionManager:
    public nx::vms::cloud_integration::AbstractCloudConnectionManager
{
public:
    DummyCloudConnectionManager(std::string cloudSystemId):
        m_cloudSystemId(cloudSystemId)
    {
    }

    virtual boost::optional<nx::hpm::api::SystemCredentials> getSystemCredentials() const override
    {
        if (m_cloudSystemId.empty())
            return boost::none;

        nx::hpm::api::SystemCredentials systemCredentials;
        systemCredentials.systemId = m_cloudSystemId.c_str();
        systemCredentials.key = nx::utils::generateRandomName(7);
        return systemCredentials;
    }

    virtual bool boundToCloud() const override
    {
        return !m_cloudSystemId.empty();
    }

    virtual std::unique_ptr<nx::cdb::api::Connection> getCloudConnection(
        const QString& /*cloudSystemId*/,
        const QString& /*cloudAuthKey*/) const override
    {
        return nullptr;
    }

    virtual std::unique_ptr<nx::cdb::api::Connection> getCloudConnection() override
    {
        return nullptr;
    }

    virtual void processCloudErrorCode(nx::cdb::api::ResultCode /*resultCode*/) override
    {
    }

    void setCloudSystemId(std::string cloudSystemId)
    {
        m_cloudSystemId = cloudSystemId;
    }

private:
    std::string m_cloudSystemId;
};

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
        m_nonce = nx::cdb::api::generateCloudNonceBase(m_cloudSystemId).c_str();
    }

private:
    boost::optional<nx::Buffer> m_nonce;
    std::string m_cloudSystemId;

    virtual void userInfoChanged(
        const nx::Buffer& /*userName*/,
        const nx::cdb::api::AuthInfo& /*authInfo*/) override
    {
    }

    virtual void userInfoRemoved(const nx::Buffer& /*userName*/) override
    {
    }
};

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
        m_cloudConnectionManager(m_cloudSystemId),
        m_cloudUserInfoPool(m_cloudSystemId),
        m_cdbNonceFetcher(
            &m_timerManager,
            &m_cloudConnectionManager,
            &m_cloudUserInfoPool,
            &m_nonceProvider)
    {
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

private:
    std::string m_cloudSystemId;
    nx::utils::StandaloneTimerManager m_timerManager;
    DummyCloudConnectionManager m_cloudConnectionManager;
    DummyCloudUserInfoPool m_cloudUserInfoPool;
    DummyNonceProvider m_nonceProvider;
    nx::vms::cloud_integration::CdbNonceFetcher m_cdbNonceFetcher;
    QByteArray m_prevNonce;
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

} // namespace test
} // namespace mediaserver
} // namespace vms
} // namespace nx
