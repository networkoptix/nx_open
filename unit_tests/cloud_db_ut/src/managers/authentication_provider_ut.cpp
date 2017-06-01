#include <gtest/gtest.h>

#include <nx/network/http/auth_tools.h>
#include <nx/utils/std/cpp14.h>

#include <cdb/cloud_nonce.h>

#include <libcloud_db/src/dao/user_authentication_data_object_factory.h>
#include <libcloud_db/src/dao/memory/dao_memory_user_authentication.h>
#include <libcloud_db/src/managers/authentication_provider.h>
#include <libcloud_db/src/settings.h>
#include <libcloud_db/src/test_support/business_data_generator.h>

#include "account_manager_stub.h"
#include "system_sharing_manager_stub.h"
#include "temporary_account_password_manager_stub.h"

namespace nx {
namespace cdb {
namespace test {

class AuthenticationProvider:
    public ::testing::Test
{
public:
    AuthenticationProvider()
    {
        m_factoryBak = dao::UserAuthenticationDataObjectFactory::instance().setCustomFunc(
            std::bind(&AuthenticationProvider::createUserAuthenticationDao, this));

        prepareTestData();

        m_authenticationProvider = std::make_unique<cdb::AuthenticationProvider>(
            m_settings,
            m_accountManager,
            &m_systemSharingManager,
            m_temporaryAccountPasswordManager);
    }

    ~AuthenticationProvider()
    {
        dao::UserAuthenticationDataObjectFactory::instance().setCustomFunc(
            std::move(m_factoryBak));
    }

protected:
    void whenSharingSystem()
    {
        api::SystemSharing sharing;
        sharing.systemId = m_system.id;
        sharing.accountEmail = m_account.email;
        sharing.accessRole = api::SystemAccessRole::owner;

        ASSERT_EQ(
            nx::db::DBResult::ok,
            m_authenticationProvider->afterSharingSystem(nullptr, sharing));
    }

    void thenValidAuthRecordIsGenerated()
    {
        auto authRecords = m_userAuthenticationDao->fetchUserAuthRecords(
            nullptr, m_system.id, m_account.email);
        ASSERT_EQ(1U, authRecords.size());
        assertUserAuthenticationRecordIsValid(authRecords[0]);
    }

    void assertUserAuthenticationRecordIsValid(const api::AuthInfo& authInfo)
    {
        const auto ha2 = nx_http::calcHa2("GET", "/getsome/");
        const auto nonce = api::generateNonce(authInfo.nonce);
        const auto expectedResponse = nx_http::calcResponse(
            m_account.passwordHa1.c_str(), nonce.c_str(), ha2);

        const auto nonceTrailer = nonce.substr(authInfo.nonce.size());

        const auto actualResponse = nx_http::calcResponseFromIntermediate(
            authInfo.intermediateResponse.c_str(),
            authInfo.nonce.size(),
            nonceTrailer.c_str(),
            ha2);

        ASSERT_EQ(expectedResponse, actualResponse);
    }

private:
    conf::Settings m_settings;
    AccountManagerStub m_accountManager;
    SystemSharingManagerStub m_systemSharingManager;
    TemporaryAccountPasswordManagerStub m_temporaryAccountPasswordManager;
    std::unique_ptr<cdb::AuthenticationProvider> m_authenticationProvider;
    dao::UserAuthenticationDataObjectFactory::Function m_factoryBak;
    dao::memory::UserAuthentication* m_userAuthenticationDao = nullptr;
    AccountWithPassword m_account;
    data::SystemData m_system;

    std::unique_ptr<dao::AbstractUserAuthentication> createUserAuthenticationDao()
    {
        auto dao = std::make_unique<dao::memory::UserAuthentication>();
        m_userAuthenticationDao = dao.get();
        return std::move(dao);
    }

    void prepareTestData()
    {
        m_account = BusinessDataGenerator::generateRandomAccount();
        m_system = BusinessDataGenerator::generateRandomSystem(m_account);

        m_accountManager.addAccount(m_account);
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(AuthenticationProvider, afterSharingSystem_adds_valid_auth_record)
{
    whenSharingSystem();
    thenValidAuthRecordIsGenerated();
}

//TEST_F(AuthenticationProvider, afterSharingSystem_removes_expired_auth_record)

//TEST_F(AuthenticationProvider, afterSharingSystem_adds_valid_auth_record)

//TEST_F(AuthenticationProvider, afterSharingSystem_uses_system_wide_nonce)

} // namespace test
} // namespace cdb
} // namespace nx
