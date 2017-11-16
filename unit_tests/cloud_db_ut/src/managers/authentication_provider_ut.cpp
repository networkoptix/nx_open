#include <array>

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/time.h>

#include <nx/cloud/cdb/api/cloud_nonce.h>

#include <nx/cloud/cdb/client/data/auth_data.h>
#include <nx/cloud/cdb/dao/user_authentication_data_object_factory.h>
#include <nx/cloud/cdb/dao/memory/dao_memory_user_authentication.h>
#include <nx/cloud/cdb/ec2/synchronization_engine.h>
#include <nx/cloud/cdb/managers/authentication_provider.h>
#include <nx/cloud/cdb/settings.h>
#include <nx/cloud/cdb/stree/cdb_ns.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>

#include "account_manager_stub.h"
#include "base_persistent_data_test.h"
#include "system_sharing_manager_stub.h"
#include "temporary_account_password_manager_stub.h"
#include "vms_p2p_command_bus_stub.h"

namespace nx {
namespace cdb {
namespace test {

class AuthenticationProvider:
    public ::testing::Test,
    public BasePersistentDataTest
{
public:
    AuthenticationProvider()
    {
        using namespace std::placeholders;

        m_factoryBak = dao::UserAuthenticationDataObjectFactory::instance().setCustomFunc(
            std::bind(&AuthenticationProvider::createUserAuthenticationDao, this));

        std::array<const char*, 1U> args{"--auth/nonceValidityPeriod=1ms"};
        m_settings.load(args.size(), args.data());

        prepareTestData();

        m_vmsP2pCommandBusStub.setOnSaveResourceAttribute(
            std::bind(&AuthenticationProvider::onSaveResourceAttribute, this, _1, _2));

        m_authenticationProvider = std::make_unique<cdb::AuthenticationProvider>(
            m_settings,
            &queryExecutor(),
            &m_accountManager,
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
    void givenAccountWithMultipleSystems()
    {
        constexpr int kSystemCount = 7;

        m_systems.clear();
        for (int i = 0; i < kSystemCount; ++i)
        {
            m_systems.push_back(BusinessDataGenerator::generateRandomSystem(m_ownerAccount));
            shareSystem(
                m_systems.back().id,
                m_ownerAccount.email,
                api::SystemAccessRole::owner);
        }
    }

    void givenSystemWithNonce()
    {
        // Nonce is created for sure while sharing.
        whenSharingSystem();
    }

    void whenSharingSystem()
    {
        shareSystem(
            m_systems[0].id,
            m_ownerAccount.email,
            api::SystemAccessRole::owner);
    }

    void whenSharingSystemWithMultipleUsers()
    {
        for (auto& account: m_additionalAccounts)
        {
            shareSystem(
                m_systems[0].id,
                account.email,
                api::SystemAccessRole::liveViewer);
        }
    }

    void whenSharingSystemWithNotExistingAccount()
    {
        shareSystem(
            m_systems[0].id,
            BusinessDataGenerator::generateRandomEmailAddress(),
            api::SystemAccessRole::liveViewer);
    }

    void whenSystemSharingRanIntoDbError()
    {
        // TODO
    }

    void whenChangingAccountPassword()
    {
        m_ownerAccount.password = nx::utils::generateRandomName(7).toStdString();
        m_ownerAccount.passwordHa1 = nx_http::calcHa1(
            m_ownerAccount.email.c_str(),
            nx::network::AppInfo::realm().toStdString().c_str(),
            m_ownerAccount.password.c_str()).toStdString();

        ASSERT_NO_THROW(
            m_authenticationProvider->afterUpdatingAccountPassword(
                nullptr, m_ownerAccount));
    }

    void whenGeneratedNonceTimeoutHasExpired()
    {
        std::this_thread::sleep_for(
            m_settings.auth().nonceValidityPeriod + std::chrono::seconds(1));
    }

    void whenRequestAuthenticationResponse()
    {
        using namespace std::placeholders;

        nx::utils::stree::ResourceContainer rc;
        rc.put(nx::cdb::attr::authSystemId, QString::fromStdString(m_systems[0].id));

        const auto nonce = m_userAuthenticationDao->fetchSystemNonce(
            nullptr, m_systems[0].id);

        data::AuthRequest authRequest;
        authRequest.nonce = *nonce;
        authRequest.realm = nx::network::AppInfo::realm().toStdString();
        authRequest.username = m_ownerAccount.email;

        m_authenticationProvider->getAuthenticationResponse(
            AuthorizationInfo(std::move(rc)),
            authRequest,
            std::bind(&AuthenticationProvider::saveAuthenticationResponse, this, _1, _2));
    }

    void thenValidAuthRecordIsGenerated()
    {
        assertUserAuthRecordsAreValidForSystem(m_systems[0].id);
    }

    void thenEachUserAuthRecordContainsSystemNonce()
    {
        boost::optional<std::string> nonce;
        for (const auto& account: m_additionalAccounts)
        {
            const auto authInfo = m_userAuthenticationDao->fetchUserAuthRecords(
                nullptr, m_systems[0].id, account.id);
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
            std::make_pair(m_systems[0].id, guidFromArbitraryData(m_ownerAccount.email)));
        ASSERT_TRUE(it != m_updateUserAuthInfoTransactions.end());
        ASSERT_EQ(1U, it->second.records.size());
        assertUserAuthenticationHashIsValid(it->second.records[0]);
    }

    void thenUpdateUserTransactionHasNotBeenGenerated()
    {
        ASSERT_TRUE(m_updateUserAuthInfoTransactions.empty());
    }

    void thenHashForEverySystemHasBeenGenerated()
    {
        for (const auto& system: m_systems)
            assertUserAuthRecordsAreValidForSystem(system.id);
    }

    void thenAuthenticationResponseIsProvided()
    {
        auto authResponse = m_authResponses.pop();
        ASSERT_EQ(api::ResultCode::ok, std::get<0>(authResponse));
    }

    void thenSystemNonceIsValidForAuthentication()
    {
        whenRequestAuthenticationResponse();
        thenAuthenticationResponseIsProvided();
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
    std::vector<AccountWithPassword> m_additionalAccounts;
    std::vector<data::SystemData> m_systems;
    VmsP2pCommandBusStub m_vmsP2pCommandBusStub;
    // map<pair<system id, user email>, >
    std::map<
        std::pair<std::string, QnUuid>,
        api::AuthInfo> m_updateUserAuthInfoTransactions;
    nx::utils::SyncQueue<std::tuple<api::ResultCode, api::AuthResponse>> m_authResponses;

    std::unique_ptr<dao::AbstractUserAuthentication> createUserAuthenticationDao()
    {
        auto dao = std::make_unique<dao::memory::UserAuthentication>();
        m_userAuthenticationDao = dao.get();
        return std::move(dao);
    }

    void prepareTestData()
    {
        m_ownerAccount = BusinessDataGenerator::generateRandomAccount();
        m_systems.push_back(BusinessDataGenerator::generateRandomSystem(m_ownerAccount));

        m_accountManager.addAccount(m_ownerAccount);

        initializeAdditionalAccounts();
    }

    void initializeAdditionalAccounts()
    {
        constexpr std::size_t kAccountNumber = 2;

        m_additionalAccounts.resize(kAccountNumber);
        for (auto& account : m_additionalAccounts)
        {
            account = BusinessDataGenerator::generateRandomAccount();
            m_accountManager.addAccount(account);
        }
    }

    void shareSystem(
        const std::string& systemId,
        const std::string& email,
        api::SystemAccessRole accessRole)
    {
        api::SystemSharingEx sharing;
        sharing.systemId = systemId;
        sharing.accountEmail = email;
        sharing.accessRole = accessRole;
        sharing.vmsUserId = guidFromArbitraryData(email).toSimpleByteArray().toStdString();

        m_systemSharingManager.add(sharing);

        ASSERT_EQ(
            nx::utils::db::DBResult::ok,
            m_authenticationProvider->afterSharingSystem(
                nullptr, sharing, SharingType::sharingWithExistingAccount));
    }

    nx::utils::db::DBResult onSaveResourceAttribute(
        const std::string& systemId,
        const ::ec2::ApiResourceParamWithRefData& data)
    {
        if (data.name == api::kVmsUserAuthInfoAttributeName)
        {
            m_updateUserAuthInfoTransactions.emplace(
                std::make_pair(systemId, data.resourceId),
                QJson::deserialized<api::AuthInfo>(data.value.toUtf8()));
        }

        return nx::utils::db::DBResult::ok;
    }

    void assertUserAuthRecordsAreValidForSystem(const std::string& systemId)
    {
        auto authInfo = m_userAuthenticationDao->fetchUserAuthRecords(
            nullptr, systemId, m_ownerAccount.id);
        ASSERT_EQ(1U, authInfo.records.size());
        assertUserAuthenticationHashIsValid(authInfo.records[0]);
        assertUserAuthenticationTimestampIsValid(authInfo.records[0]);
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

    void saveAuthenticationResponse(
        api::ResultCode resultCode,
        api::AuthResponse authResponse)
    {
        m_authResponses.push(std::make_tuple(resultCode, authResponse));
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
    ASSERT_THROW(whenSharingSystemWithNotExistingAccount(), nx::utils::db::Exception);
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

TEST_F(AuthenticationProvider, each_system_auth_hash_is_updated_on_account_password_change)
{
    givenAccountWithMultipleSystems();
    whenChangingAccountPassword();
    thenHashForEverySystemHasBeenGenerated();
}

TEST_F(AuthenticationProvider, system_nonce_is_always_valid)
{
    givenSystemWithNonce();
    whenGeneratedNonceTimeoutHasExpired();
    thenSystemNonceIsValidForAuthentication();
}

} // namespace test
} // namespace cdb
} // namespace nx
