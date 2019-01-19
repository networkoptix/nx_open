#include <future>

#include <gtest/gtest.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <nx/utils/crc32.h>
#include <nx/utils/time.h>

#include <nx/cloud/db/managers/temporary_account_password_manager.h>
#include <nx/cloud/db/dao/memory/dao_memory_temporary_credentials.h>
#include <nx/cloud/db/stree/cdb_ns.h>
#include <nx/cloud/db/test_support/base_persistent_data_test.h>
#include <nx/cloud/db/test_support/business_data_generator.h>

namespace nx::cloud::db::test {

class TemporaryAccountPasswordManager:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    TemporaryAccountPasswordManager():
        m_timeShift(nx::utils::test::ClockType::system),
        m_expirationPeriod(7),
        m_prolongationPeriod(std::chrono::hours(24))
    {
        m_settings.removeExpiredTemporaryCredentialsPeriod = std::chrono::milliseconds(1);

        initializeTempPasswordManager();

        m_email = BusinessDataGenerator::generateRandomEmailAddress();
    }

protected:
    std::chrono::seconds expirationPeriod() const
    {
        return m_expirationPeriod;
    }

    std::chrono::seconds prolongationPeriod() const
    {
        return *m_prolongationPeriod;
    }

    void generateTemporaryCredentials()
    {
        using namespace std::chrono;

        m_credentials.accountEmail = m_email;
        m_credentials.expirationTimestampUtc = 
            duration_cast<seconds>(nx::utils::timeSinceEpoch() + m_expirationPeriod).count();
        m_credentials.maxUseCount = 100;
        if (m_prolongationPeriod)
        {
            m_credentials.prolongationPeriodSec =
                duration_cast<seconds>(*m_prolongationPeriod).count();
        }
        m_tempPasswordManager->addRandomCredentials(m_email, &m_credentials);
    }

    void addTemporaryCredentials()
    {
        generateTemporaryCredentials();

        std::promise<api::Result> done;
        m_tempPasswordManager->registerTemporaryCredentials(
            AuthorizationInfo(),
            m_credentials,
            [&done](api::Result result) { done.set_value(result); });
        ASSERT_EQ(api::ResultCode::ok, done.get_future().get().code);
    }

    void givenProlongatedCredentials()
    {
        addTemporaryCredentials();
        whenPeriodPasses(expirationPeriod() / 2);
        assertCredentialsAreAuthenticated();
    }

    void whenExpirationPeriodPasses()
    {
        whenPeriodPasses(m_expirationPeriod);
    }

    void whenPeriodPasses(std::chrono::seconds period)
    {
        m_timeShift.applyRelativeShift(period);
    }

    void thenCredentialsAreRemoved()
    {
        assertCredentialsAreNotAuthenticated();
        waitUntilCredentialsArePermanentlyDeleted();
    }

    void assertCredentialsAreAuthenticated()
    {
        ASSERT_EQ(api::ResultCode::ok, authenticateCredentials());
    }

    void assertCredentialsAreNotAuthenticated()
    {
        ASSERT_NE(api::ResultCode::ok, authenticateCredentials());
    }

    void whenRestart()
    {
        m_tempPasswordManager.reset();
        initializeTempPasswordManager();
    }

    void thenCredentialsAreStillProlongated()
    {
        whenPeriodPasses(prolongationPeriod());
        assertCredentialsAreAuthenticated();
    }
    
    void waitUntilCredentialsArePermanentlyDeleted()
    {
        for (;;)
        {
            const auto credentials =
                m_dao.find(nullptr, nx::utils::stree::ResourceNameSet(), m_credentials);

            if (!credentials)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void assertCredentialsLoginContainsAccountEmailCrc32()
    {
        std::vector<std::string> splitVec;
        boost::split(splitVec, m_credentials.login, boost::algorithm::is_any_of("-"));
        ASSERT_EQ(2U, splitVec.size());
        ASSERT_EQ(std::to_string(nx::utils::crc32(m_email)), splitVec[1]);
    }

private:
    CdbAttrNameSet m_attrNameset;
    conf::AccountManager m_settings;
    dao::memory::TemporaryCredentialsDao m_dao;
    std::unique_ptr<nx::cloud::db::TemporaryAccountPasswordManager> m_tempPasswordManager;
    data::TemporaryAccountCredentials m_credentials;
    std::string m_email;
    nx::utils::test::ScopedTimeShift m_timeShift;
    const std::chrono::hours m_expirationPeriod;
    std::optional<std::chrono::seconds> m_prolongationPeriod;

    void initializeTempPasswordManager()
    {
        m_tempPasswordManager =
            std::make_unique<nx::cloud::db::TemporaryAccountPasswordManager>(
                m_settings,
                m_attrNameset,
                &persistentDbManager()->queryExecutor(),
                &m_dao);
    }

    api::ResultCode authenticateCredentials()
    {
        nx::utils::stree::ResourceContainer authProperties;
        authProperties.put(attr::requestPath, "/any/request/path");

        std::promise<api::Result> done;
        m_tempPasswordManager->authenticateByName(
            m_credentials.login.c_str(),
            [](auto...) { return true; }, //< Password check functor.
            &authProperties,
            [&done](api::Result result) { done.set_value(result); });

        const auto resultCode = done.get_future().get().code;
        if (resultCode != api::ResultCode::ok)
            return resultCode;

        if (!m_tempPasswordManager->authorize(
                *authProperties.get<std::string>(attr::credentialsId),
                authProperties))
        {
            return api::ResultCode::notAuthorized;
        }

        return api::ResultCode::ok;
    }
};

TEST_F(TemporaryAccountPasswordManager, temporary_login_is_equal_to_account_email)
{
    generateTemporaryCredentials();
    assertCredentialsLoginContainsAccountEmailCrc32();
}

TEST_F(TemporaryAccountPasswordManager, credentials_work)
{
    addTemporaryCredentials();
    assertCredentialsAreAuthenticated();
}

TEST_F(TemporaryAccountPasswordManager, credentials_are_removed_after_expiration)
{
    addTemporaryCredentials();
    whenExpirationPeriodPasses();
    waitUntilCredentialsArePermanentlyDeleted();
}

TEST_F(TemporaryAccountPasswordManager, credentials_cannot_be_used_after_expiration)
{
    addTemporaryCredentials();
    whenExpirationPeriodPasses();
    assertCredentialsAreNotAuthenticated();
}

TEST_F(TemporaryAccountPasswordManager, credentials_prolongated_automatically)
{
    addTemporaryCredentials();
   
    whenPeriodPasses(expirationPeriod() / 2);
    assertCredentialsAreAuthenticated();

    whenPeriodPasses(expirationPeriod() / 2);
    assertCredentialsAreAuthenticated();
}

TEST_F(TemporaryAccountPasswordManager, prolongation_is_persistent)
{
    givenProlongatedCredentials();
    whenRestart();
    thenCredentialsAreStillProlongated();
}

} // namespace nx::cloud::db::test
