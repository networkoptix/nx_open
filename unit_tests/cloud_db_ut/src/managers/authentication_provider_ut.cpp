#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/time.h>

#include <utils/db/test_support/test_with_db_helper.h>

#include <nx/cloud/cdb/api/cloud_nonce.h>

#include <nx/cloud/cdb/dao/user_authentication_data_object_factory.h>
#include <nx/cloud/cdb/dao/memory/dao_memory_user_authentication.h>
#include <nx/cloud/cdb/ec2/synchronization_engine.h>
#include <nx/cloud/cdb/managers/authentication_provider.h>
#include <nx/cloud/cdb/settings.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>
#include <nx/cloud/cdb/client/data/auth_data.h>

#include "account_manager_stub.h"
#include "system_sharing_manager_stub.h"
#include "vms_p2p_command_bus_stub.h"
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
        using namespace std::placeholders;

        m_factoryBak = dao::UserAuthenticationDataObjectFactory::instance().setCustomFunc(
            std::bind(&AuthenticationProvider::createUserAuthenticationDao, this));

        prepareTestData();
        
        m_vmsP2pCommandBusStub.setOnSaveResourceAttribute(
            std::bind(&AuthenticationProvider::onSaveResourceAttribute, this, _1, _2));

        m_authenticationProvider = std::make_unique<cdb::AuthenticationProvider>(
            m_settings,
            m_accountManager,
            &m_systemSharingManager,
            m_temporaryAccountPasswordManager,
            &m_vmsP2pCommandBusStub);
    }

    ~AuthenticationProvider()
    {
        dao::UserAuthenticationDataObjectFactory::instance().setCustomFunc(
            std::move(m_factoryBak));
    }

protected:
    void whenSharingSystem()
    {
        shareSystem(m_ownerAccount.email, api::SystemAccessRole::owner);
    }

    void whenSharingSystemWithMultipleUsers()
    {
        constexpr std::size_t kAccountNumber = 2;

        m_accounts.resize(kAccountNumber);
        for (auto& account: m_accounts)
        {
            account = BusinessDataGenerator::generateRandomAccount();
            m_accountManager.addAccount(account);
            shareSystem(account.email, api::SystemAccessRole::liveViewer);
        }
    }

    void whenSharingSystemWithNotExistingAccount()
    {
        shareSystem(
            BusinessDataGenerator::generateRandomEmailAddress(),
            api::SystemAccessRole::liveViewer);
    }

    void whenSystemSharingRanIntoDbError()
    {
        // TODO
    }

    void thenValidAuthRecordIsGenerated()
    {
        auto authInfo = m_userAuthenticationDao->fetchUserAuthRecords(
            nullptr, m_system.id, m_ownerAccount.email);
        ASSERT_EQ(1U, authInfo.records.size());
        assertUserAuthenticationHashIsValid(authInfo.records[0]);
        assertUserAuthenticationTimestampIsValid(authInfo.records[0]);
    }

    void thenEachUserAuthRecordContainsSystemNonce()
    {
        boost::optional<std::string> nonce;
        for (const auto& account: m_accounts)
        {
            const auto authInfo = m_userAuthenticationDao->fetchUserAuthRecords(
                nullptr, m_system.id, account.email);
            ASSERT_FALSE(authInfo.records.empty());

            for (const auto& authRecord: authInfo.records)
            {
                if (nonce)
                    ASSERT_EQ(*nonce, authRecord.nonce);
                else
                    nonce = authRecord.nonce;
            }
        }
    }

    void thenUpdateUserTransactionHasBeenGenerated()
    {
        auto it = m_updateUserAuthInfoTransactions.find(
            std::make_pair(m_system.id, guidFromArbitraryData(m_ownerAccount.email)));
        ASSERT_TRUE(it != m_updateUserAuthInfoTransactions.end());
        ASSERT_EQ(1U, it->second.records.size());
        assertUserAuthenticationHashIsValid(it->second.records[0]);
    }

    void thenUpdateUserTransactionHasNotBeenGenerated()
    {
        ASSERT_TRUE(m_updateUserAuthInfoTransactions.empty());
    }

    void assertUserAuthenticationHashIsValid(const api::AuthInfoRecord& authInfo)
    {
        const auto ha2 = nx_http::calcHa2("GET", "/getsome/");
        const auto nonce = api::generateNonce(authInfo.nonce);
        const auto expectedResponse = nx_http::calcResponse(
            m_ownerAccount.passwordHa1.c_str(), nonce.c_str(), ha2);

        const auto nonceTrailer = nonce.substr(authInfo.nonce.size());

        const auto actualResponse = nx_http::calcResponseFromIntermediate(
            authInfo.intermediateResponse.c_str(),
            authInfo.nonce.size(),
            nonceTrailer.c_str(),
            ha2);

        ASSERT_EQ(expectedResponse, actualResponse);
    }

    void assertUserAuthenticationTimestampIsValid(const api::AuthInfoRecord& authInfo)
    {
        using namespace std::chrono;

        ASSERT_GT(
            nx::utils::floor<milliseconds>(authInfo.expirationTime),
            nx::utils::floor<milliseconds>(nx::utils::utcTime()));
        ASSERT_LE(
            nx::utils::floor<milliseconds>(authInfo.expirationTime),
            nx::utils::floor<milliseconds>(
                nx::utils::utcTime() + m_settings.auth().offlineUserHashValidityPeriod));
    }

private:
    conf::Settings m_settings;
    AccountManagerStub m_accountManager;
    SystemSharingManagerStub m_systemSharingManager;
    TemporaryAccountPasswordManagerStub m_temporaryAccountPasswordManager;
    std::unique_ptr<cdb::AuthenticationProvider> m_authenticationProvider;
    dao::UserAuthenticationDataObjectFactory::Function m_factoryBak;
    dao::memory::UserAuthentication* m_userAuthenticationDao = nullptr;
    AccountWithPassword m_ownerAccount;
    std::vector<AccountWithPassword> m_accounts;
    data::SystemData m_system;
    VmsP2pCommandBusStub m_vmsP2pCommandBusStub;
    // map<pair<system id, user email>, >
    std::map<
        std::pair<std::string, QnUuid>,
        api::AuthInfo> m_updateUserAuthInfoTransactions;

    std::unique_ptr<dao::AbstractUserAuthentication> createUserAuthenticationDao()
    {
        auto dao = std::make_unique<dao::memory::UserAuthentication>();
        m_userAuthenticationDao = dao.get();
        return std::move(dao);
    }

    void prepareTestData()
    {
        m_ownerAccount = BusinessDataGenerator::generateRandomAccount();
        m_system = BusinessDataGenerator::generateRandomSystem(m_ownerAccount);

        m_accountManager.addAccount(m_ownerAccount);
    }

    void shareSystem(const std::string& email, api::SystemAccessRole accessRole)
    {
        api::SystemSharing sharing;
        sharing.systemId = m_system.id;
        sharing.accountEmail = email;
        sharing.accessRole = accessRole;
        sharing.vmsUserId = guidFromArbitraryData(email).toSimpleByteArray().toStdString();

        ASSERT_EQ(
            nx::db::DBResult::ok,
            m_authenticationProvider->afterSharingSystem(
                nullptr, sharing, SharingType::sharingWithExistingAccount));
    }

    nx::db::DBResult onSaveResourceAttribute(
        const std::string& systemId,
        const ::ec2::ApiResourceParamWithRefData& data)
    {
        if (data.name == api::kVmsUserAuthInfoAttributeName)
        {
            m_updateUserAuthInfoTransactions.emplace(
                std::make_pair(systemId, data.resourceId),
                QJson::deserialized<api::AuthInfo>(data.value.toUtf8()));
        }

        return nx::db::DBResult::ok;
    }
};

//-------------------------------------------------------------------------------------------------
// Test cases.

TEST_F(AuthenticationProvider, afterSharingSystem_adds_valid_auth_record)
{
    whenSharingSystem();
    thenValidAuthRecordIsGenerated();
}

TEST_F(AuthenticationProvider, afterSharingSystem_uses_system_wide_nonce_for_every_user)
{
    whenSharingSystemWithMultipleUsers();
    thenEachUserAuthRecordContainsSystemNonce();
}

TEST_F(AuthenticationProvider, afterSharingSystem_throws_on_not_found_account)
{
    ASSERT_THROW(whenSharingSystemWithNotExistingAccount(), nx::db::Exception);
}

TEST_F(AuthenticationProvider, afterSharingSystem_generates_update_user_transaction)
{
    whenSharingSystem();
    thenUpdateUserTransactionHasBeenGenerated();
}

TEST_F(
    AuthenticationProvider,
    afterSharingSystem_no_transaction_generated_after_save_to_db_failure)
{
    whenSystemSharingRanIntoDbError();
    thenUpdateUserTransactionHasNotBeenGenerated();
}

} // namespace test
} // namespace cdb
} // namespace nx
