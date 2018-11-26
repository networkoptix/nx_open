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
        m_expirationPeriod(7)
    {
        m_factoryFuncBak = dao::TemporaryCredentialsDaoFactory::instance()
            .setCustomFunc([this]() { return createDao(); });

        m_tempPasswordManager =
            std::make_unique<nx::cloud::db::TemporaryAccountPasswordManager>(
                m_attrNameset,
                &persistentDbManager()->queryExecutor());

        m_email = BusinessDataGenerator::generateRandomEmailAddress();
    }

    ~TemporaryAccountPasswordManager()
    {
        dao::TemporaryCredentialsDaoFactory::instance().setCustomFunc(
            std::exchange(m_factoryFuncBak, nullptr));
    }

protected:
    void generateTemporaryCredentials()
    {
        using namespace std::chrono;

        m_credentials.accountEmail = m_email;
        m_credentials.expirationTimestampUtc = 
            duration_cast<seconds>(nx::utils::timeSinceEpoch() + m_expirationPeriod).count();
        m_credentials.maxUseCount = 100;
        m_tempPasswordManager->addRandomCredentials(m_email, &m_credentials);
    }

    void addTemporaryCredentials()
    {
        generateTemporaryCredentials();

        std::promise<api::ResultCode> done;
        m_tempPasswordManager->registerTemporaryCredentials(
            AuthorizationInfo(),
            m_credentials,
            [&done](api::ResultCode resultCode) { done.set_value(resultCode); });
        ASSERT_EQ(api::ResultCode::ok, done.get_future().get());
    }

    void whenExpirationPeriodPasses()
    {
        m_timeShift.applyRelativeShift(m_expirationPeriod);
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

    api::ResultCode authenticateCredentials()
    {
        nx::utils::stree::ResourceContainer authProperties;

        std::promise<api::ResultCode> done;
        m_tempPasswordManager->authenticateByName(
            m_credentials.login.c_str(),
            [](auto...) { return true; }, //< Password check functor.
            &authProperties,
            [&done](api::ResultCode result) { done.set_value(result); });
        return done.get_future().get();
    }
    
    void waitUntilCredentialsArePermanentlyDeleted()
    {
        for (;;)
        {
            const auto credentials =
                m_dao->find(nullptr, nx::utils::stree::ResourceNameSet(), m_credentials);

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
    std::unique_ptr<nx::cloud::db::TemporaryAccountPasswordManager> m_tempPasswordManager;
    data::TemporaryAccountCredentials m_credentials;
    std::string m_email;
    nx::utils::test::ScopedTimeShift m_timeShift;
    const std::chrono::hours m_expirationPeriod;
    dao::TemporaryCredentialsDaoFactory::Function m_factoryFuncBak;
    dao::memory::TemporaryCredentialsDao* m_dao = nullptr;

    std::unique_ptr<dao::AbstractTemporaryCredentialsDao> createDao()
    {
        auto dao = std::make_unique<dao::memory::TemporaryCredentialsDao>();
        m_dao = dao.get();
        return dao;
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

TEST_F(TemporaryAccountPasswordManager, DISABLED_credentials_are_removed_after_expiration)
{
    addTemporaryCredentials();
    whenExpirationPeriodPasses();
    waitUntilCredentialsArePermanentlyDeleted();
}

} // namespace nx::cloud::db::test
