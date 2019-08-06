#include <array>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/time.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/db/dao/account_data_object.h>
#include <nx/cloud/db/dao/memory/dao_memory_account_data_object.h>
#include <nx/cloud/db/managers/account_manager.h>
#include <nx/cloud/db/managers/temporary_account_password_manager.h>
#include <nx/cloud/db/stree/stree_manager.h>
#include <nx/cloud/db/test_support/business_data_generator.h>
#include <nx/cloud/db/test_support/base_persistent_data_test.h>

#include "temporary_account_password_manager_stub.h"
#include "../functional_tests/test_email_manager.h"

namespace nx::cloud::db {
namespace test {

class AccountManager:
    public ::testing::Test,
    public BasePersistentDataTest,
    public AbstractAccountManagerExtension
{
public:
    AccountManager():
        m_streeManager(m_settings.auth().rulesXmlPath),
        m_emailManager(nullptr),
        m_testStartTime(nx::utils::utcTime())
    {
        m_factoryBak = dao::AccountDataObjectFactory::instance().setCustomFunc(
            []() { return std::make_unique<dao::memory::AccountDataObject>(); });

        std::string accountActivationCodeExpirationTimeoutStr =
            lm("%1s").arg(rand()).toStdString();
        std::string passwordResetCodeExpirationTimeoutStr =
            lm("%1s").arg(rand()).toStdString();
        std::array<const char*, 4> args = {
            "-accountManager/accountActivationCodeExpirationTimeout",
            accountActivationCodeExpirationTimeoutStr.c_str(),
            "-accountManager/passwordResetCodeExpirationTimeout",
            passwordResetCodeExpirationTimeoutStr.c_str()
        };

        m_settings.load(static_cast<std::size_t>(args.size()), args.data());

        m_accountManager = std::make_unique<nx::cloud::db::AccountManager>(
            m_settings,
            m_streeManager,
            &m_tempPasswordManager,
            &persistentDbManager()->queryExecutor(),
            &m_emailManager);

        m_accountManager->addExtension(this);
    }

    ~AccountManager()
    {
        dao::AccountDataObjectFactory::instance().setCustomFunc(
            std::move(m_factoryBak));
    }

protected:
    void givenActivatedAccount()
    {
        addActivatedAccount(&m_account);
    }

    void whenUpdatedAccountPassword()
    {
        using namespace std::placeholders;

        using F = void (nx::cloud::db::AccountManager::*)(
            const AuthorizationInfo& authzInfo,
            data::AccountUpdateData accountData,
            std::function<void(api::Result)> completionHandler);

        nx::utils::stree::ResourceContainer authorizationResources;
        authorizationResources.put(attr::secureSource, true);
        authorizationResources.put(
            attr::authAccountEmail,
            QString::fromStdString(m_account.email));
        AuthorizationInfo authzInfo(std::move(authorizationResources));

        data::AccountUpdateData accountData;
        accountData.passwordHa1 = nx::utils::generateRandomName(7).toStdString();

        auto [result] = makeSyncCall<api::Result>(
            std::bind(static_cast<F>(&nx::cloud::db::AccountManager::updateAccount),
                m_accountManager.get(), authzInfo, accountData, _1));
        ASSERT_EQ(api::ResultCode::ok, result.code);
    }

    void whenResetPassword()
    {
        using namespace std::placeholders;

        using ResetPasswordFunc = void(nx::cloud::db::AccountManager::*)(
            const AuthorizationInfo&,
            data::AccountEmail,
            std::function<void(api::Result, data::AccountConfirmationCode)>);

        nx::utils::stree::ResourceContainer authorizationResources;
        authorizationResources.put(attr::secureSource, true);
        AuthorizationInfo authzInfo(std::move(authorizationResources));

        data::AccountEmail accountEmail;
        accountEmail.email = m_account.email;

        auto [result, accountConfirmationCode] = 
            makeSyncCall<api::Result, data::AccountConfirmationCode>(
                std::bind(static_cast<ResetPasswordFunc>(&nx::cloud::db::AccountManager::resetPassword),
                    m_accountManager.get(), authzInfo, accountEmail, _1));
        ASSERT_EQ(api::ResultCode::ok, result.code);
    }

    void thenExtensionHasBeenInvoked()
    {
        m_extensionCalls.pop();
    }

    template<typename Duration>
    void assertEqualWithError(
        std::chrono::system_clock::time_point expected,
        std::chrono::system_clock::time_point actual,
        Duration error)
    {
        ASSERT_GE(expected, actual - error);
        ASSERT_LE(expected, actual + error);
    }

    void thenProperTemporaryCodeIsCreated()
    {
        ASSERT_FALSE(m_tempPasswordManager.registeredCredentials().empty());
        const auto& tempCredentials = m_tempPasswordManager.registeredCredentials().back();
        ASSERT_EQ(1, tempCredentials.maxUseCount);

        const auto curTime = nx::utils::utcTime();
        assertEqualWithError(
            nx::utils::floor<std::chrono::seconds>(m_testStartTime) +
                m_settings.accountManager().passwordResetCodeExpirationTimeout,
            std::chrono::system_clock::from_time_t(tempCredentials.expirationTimestampUtc),
            curTime - m_testStartTime + std::chrono::seconds(1));
        //< 1 second is to compensate round error.
    }

private:
    dao::AccountDataObjectFactory::Function m_factoryBak;
    conf::Settings m_settings;
    StreeManager m_streeManager;
    TemporaryAccountPasswordManagerStub m_tempPasswordManager;
    TestEmailManager m_emailManager;
    std::unique_ptr<nx::cloud::db::AccountManager> m_accountManager;
    nx::utils::SyncQueue<api::AccountData> m_extensionCalls;
    std::chrono::system_clock::time_point m_testStartTime;

    data::AccountData m_account;

    virtual void afterUpdatingAccountPassword(
        nx::sql::QueryContext* const /*queryContext*/,
        const api::AccountData& account)
    {
        m_extensionCalls.push(account);
    }

    void addActivatedAccount(data::AccountData* account)
    {
        const auto accountEmail = BusinessDataGenerator::generateRandomEmailAddress();

        data::AccountConfirmationCode accountConfirmationCode;
        registerAccount(accountEmail, &accountConfirmationCode);
        activateAccount(accountConfirmationCode);
        getAccount(accountEmail, account);
    }

    void registerAccount(
        const std::string& accountEmail,
        data::AccountConfirmationCode* accountConfirmationCode)
    {
        using namespace std::placeholders;

        nx::utils::stree::ResourceContainer authorizationResources;
        authorizationResources.put(attr::secureSource, true);
        AuthorizationInfo authzInfo(std::move(authorizationResources));

        data::AccountRegistrationData accountRegistrationData;
        accountRegistrationData.email = accountEmail;
        accountRegistrationData.customization = "default";
        accountRegistrationData.fullName = "H Z";
        accountRegistrationData.passwordHa1 = nx::utils::generateRandomName(22).toStdString();

        api::Result result;
        std::tie(result, *accountConfirmationCode) =
            makeSyncCall<api::Result, data::AccountConfirmationCode>(
                std::bind(&nx::cloud::db::AccountManager::registerAccount, m_accountManager.get(),
                    authzInfo, accountRegistrationData, _1));
        ASSERT_EQ(api::ResultCode::ok, result.code);
    }

    void activateAccount(const data::AccountConfirmationCode& accountConfirmationCode)
    {
        using namespace std::placeholders;

        nx::utils::stree::ResourceContainer authorizationResources;
        authorizationResources.put(attr::secureSource, true);
        AuthorizationInfo authzInfo(std::move(authorizationResources));

        auto [result, accountEmail] =
            makeSyncCall<api::Result, api::AccountEmail>(
                std::bind(&nx::cloud::db::AccountManager::activate, m_accountManager.get(),
                    authzInfo, accountConfirmationCode, _1));
        ASSERT_EQ(api::ResultCode::ok, result.code);
    }

    void getAccount(const std::string& accountEmail, data::AccountData* account)
    {
        using namespace std::placeholders;

        nx::utils::stree::ResourceContainer authorizationResources;
        authorizationResources.put(attr::authAccountEmail, QString::fromStdString(accountEmail));
        AuthorizationInfo authzInfo(std::move(authorizationResources));

        api::Result result;
        std::tie(result, *account) =
            makeSyncCall<api::Result, api::AccountData>(
                std::bind(&nx::cloud::db::AccountManager::getAccount, m_accountManager.get(),
                    authzInfo, _1));
        ASSERT_EQ(api::ResultCode::ok, result.code);
    }
};

TEST_F(AccountManager, invokes_extension_while_updating_account_password)
{
    givenActivatedAccount();
    whenUpdatedAccountPassword();
    thenExtensionHasBeenInvoked();
}

// TEST_F(AccountManager, reports_error_when_extension_throws)

TEST_F(AccountManager, correct_password_reset_code)
{
    givenActivatedAccount();
    whenResetPassword();
    thenProperTemporaryCodeIsCreated();
}

} // namespace test
} // namespace nx::cloud::db
