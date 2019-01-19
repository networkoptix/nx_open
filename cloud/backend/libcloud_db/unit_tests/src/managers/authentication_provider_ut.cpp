#include <array>

#include <gtest/gtest.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/time.h>
#include <nx/utils/test_support/settings_loader.h>

#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/cloud/db/api/cloud_nonce.h>
#include <nx/cloud/db/client/data/auth_data.h>
#include <nx/cloud/db/dao/user_authentication_data_object_factory.h>
#include <nx/cloud/db/dao/memory/dao_memory_user_authentication.h>
#include <nx/cloud/db/managers/authentication_provider.h>
#include <nx/cloud/db/settings.h>
#include <nx/cloud/db/stree/cdb_ns.h>
#include <nx/cloud/db/test_support/base_persistent_data_test.h>
#include <nx/cloud/db/test_support/business_data_generator.h>

#include "account_manager_stub.h"
#include "system_sharing_manager_stub.h"
#include "temporary_account_password_manager_stub.h"
#include "vms_p2p_command_bus_stub.h"

namespace nx::cloud::db {
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

        m_settingsLoader.addArg("--auth/nonceValidityPeriod=1ms");
        m_settingsLoader.addArg("--auth/checkForExpiredAuthPeriod=10ms");
        m_settingsLoader.addArg("--auth/continueUpdatingExpiredAuthPeriod=1ms");
        m_settingsLoader.load();

        prepareTestData();

        m_vmsP2pCommandBusStub.setOnSaveResourceAttribute(
            std::bind(&AuthenticationProvider::onSaveResourceAttribute, this, _1, _2));

        m_authenticationProvider = std::make_unique<nx::cloud::db::AuthenticationProvider>(
            m_settingsLoader.settings(),
            &queryExecutor(),
            &m_accountManager,
            &m_systemSharingManager,
            m_temporaryAccountPasswordManager,
            &m_vmsP2pCommandBusStub);
    }

    ~AuthenticationProvider()
    {
        m_authenticationProvider.reset();

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

    AccountWithPassword addUserWithoutPassword()
    {
        auto account = BusinessDataGenerator::generateRandomAccount();
        account.statusCode = api::AccountStatus::invited;
        account.passwordHa1.clear();
        account.passwordHa1Sha256.clear();
        account.password.clear();

        m_accountManager.addAccount(account);

        shareSystem(m_systems[0].id, account.email, api::SystemAccessRole::cloudAdmin);

        return account;
    }

    AccountWithPassword addUserWithShortAuthInfoExpirationPeriod()
    {
        return addUserWithAuthInfoExpirationPeriod(std::chrono::seconds(1));
    }

    AccountWithPassword addUserWithLongAuthInfoExpirationPeriod()
    {
        return addUserWithAuthInfoExpirationPeriod(7 * std::chrono::hours(24));
    }

    AccountWithPassword addUserWithAuthInfoExpirationPeriod(
        std::chrono::seconds expirationPeriod)
    {
        m_settingsLoader.addArg(
            "-auth/offlineUserHashValidityPeriod",
            std::to_string(expirationPeriod.count()));
        m_settingsLoader.load();

        auto account = BusinessDataGenerator::generateRandomAccount();
        m_accountManager.addAccount(account);

        shareSystem(m_systems[0].id, account.email, api::SystemAccessRole::cloudAdmin);

        m_accountToUserAuthInfo[account.email] =
            m_userAuthenticationDao->fetchUserAuthRecords(
                nullptr, m_systems[0].id, account.id);

        return account;
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
        m_ownerAccount.passwordHa1 = nx::network::http::calcHa1(
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
            m_settingsLoader.settings().auth().nonceValidityPeriod + std::chrono::seconds(1));
    }

    void whenRequestAuthenticationResponse()
    {
        using namespace std::placeholders;

        nx::utils::stree::ResourceContainer rc;
        rc.put(nx::cloud::db::attr::authSystemId, QString::fromStdString(m_systems[0].id));

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
        std::optional<std::string> nonce;
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
        ASSERT_EQ(api::ResultCode::ok, std::get<0>(authResponse).code);
    }

    void thenSystemNonceIsValidForAuthentication()
    {
        whenRequestAuthenticationResponse();
        thenAuthenticationResponseIsProvided();
    }

    void waitForUserAuthRecordToBeUpdated(const AccountWithPassword& userAccount)
    {
        const api::AuthInfo initialUserAuth = m_accountToUserAuthInfo[userAccount.email];

        for (;;)
        {
            const auto userAuthInfo = m_userAuthenticationDao->fetchUserAuthRecords(
                nullptr, m_systems[0].id, userAccount.id);
            if (!userAuthInfo.records.empty() && userAuthInfo != initialUserAuth)
            {
                ASSERT_EQ(1U, userAuthInfo.records.size());
                ASSERT_GT(
                    userAuthInfo.records.front().expirationTime,
                    initialUserAuth.records.front().expirationTime);
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void assertUserDoesNotHaveAuthRecord(const AccountWithPassword& userAccount)
    {
        const auto userAuthInfo = m_userAuthenticationDao->fetchUserAuthRecords(
            nullptr, m_systems[0].id, userAccount.id);
        ASSERT_TRUE(userAuthInfo.records.empty());
    }

private:
    nx::utils::test::SettingsLoader<conf::Settings> m_settingsLoader;
    AccountManagerStub m_accountManager;
    SystemSharingManagerStub m_systemSharingManager;
    TemporaryAccountPasswordManagerStub m_temporaryAccountPasswordManager;
    std::unique_ptr<nx::cloud::db::AuthenticationProvider> m_authenticationProvider;
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
    nx::utils::SyncQueue<std::tuple<api::Result, api::AuthResponse>> m_authResponses;
    std::map<std::string /*email*/, api::AuthInfo> m_accountToUserAuthInfo;

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
            nx::sql::DBResult::ok,
            m_authenticationProvider->afterSharingSystem(
                nullptr, sharing, SharingType::sharingWithExistingAccount));
    }

    nx::sql::DBResult onSaveResourceAttribute(
        const std::string& systemId,
        const nx::vms::api::ResourceParamWithRefData& data)
    {
        if (data.name == api::kVmsUserAuthInfoAttributeName)
        {
            m_updateUserAuthInfoTransactions.emplace(
                std::make_pair(systemId, data.resourceId),
                QJson::deserialized<api::AuthInfo>(data.value.toUtf8()));
        }

        return nx::sql::DBResult::ok;
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
        const auto ha2 = nx::network::http::calcHa2("GET", "/getsome/");
        const auto nonce = api::generateNonce(authInfo.nonce);
        const auto expectedResponse = nx::network::http::calcResponse(
            m_ownerAccount.passwordHa1.c_str(), nonce.c_str(), ha2);

        const auto nonceTrailer = nonce.substr(authInfo.nonce.size());

        const auto actualResponse = nx::network::http::calcResponseFromIntermediate(
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
                nx::utils::utcTime() +
                m_settingsLoader.settings().auth().offlineUserHashValidityPeriod));
    }

    void saveAuthenticationResponse(
        api::Result result,
        api::AuthResponse authResponse)
    {
        m_authResponses.push(std::make_tuple(result, authResponse));
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
    ASSERT_THROW(whenSharingSystemWithNotExistingAccount(), nx::sql::Exception);
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

TEST_F(AuthenticationProvider, expired_auth_record_is_updated_eventually)
{
    givenSystemWithNonce();
    const auto user1 = addUserWithShortAuthInfoExpirationPeriod();
    const auto user2 = addUserWithLongAuthInfoExpirationPeriod();

    waitForUserAuthRecordToBeUpdated(user1);
    waitForUserAuthRecordToBeUpdated(user2);
}

// E.g., invited but not yet activated account does not have password.
TEST_F(AuthenticationProvider, does_not_try_to_calculate_auth_record_for_user_without_password)
{
    givenSystemWithNonce();

    const auto userWithoutPassword = addUserWithoutPassword();
    const auto userWithShortAuthInfoExpirationPeriod =
        addUserWithShortAuthInfoExpirationPeriod();

    waitForUserAuthRecordToBeUpdated(userWithShortAuthInfoExpirationPeriod);
    assertUserDoesNotHaveAuthRecord(userWithoutPassword);
}

} // namespace test
} // namespace nx::cloud::db
