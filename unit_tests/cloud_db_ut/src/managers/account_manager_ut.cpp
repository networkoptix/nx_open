#include <array>

#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/time.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/dao/account_data_object.h>
#include <nx/cloud/cdb/dao/memory/dao_memory_account_data_object.h>
#include <nx/cloud/cdb/managers/account_manager.h>
#include <nx/cloud/cdb/managers/temporary_account_password_manager.h>
#include <nx/cloud/cdb/stree/stree_manager.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>

#include "base_persistent_data_test.h"
#include "temporary_account_password_manager_stub.h"
#include "../functional_tests/test_email_manager.h"

namespace nx {
namespace cdb {
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

        m_accountManager = std::make_unique<cdb::AccountManager>(
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

        using F = void (cdb::AccountManager::*)(
            const AuthorizationInfo& authzInfo,
            data::AccountUpdateData accountData,
            std::function<void(api::ResultCode)> completionHandler);

        nx::utils::stree::ResourceContainer authorizationResources;
        authorizationResources.put(attr::secureSource, true);
        authorizationResources.put(
            attr::authAccountEmail,
            QString::fromStdString(m_account.email));
        AuthorizationInfo authzInfo(std::move(authorizationResources));

        data::AccountUpdateData accountData;
        accountData.passwordHa1 = nx::utils::generateRandomName(7).toStdString();

        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode) =
            makeSyncCall<api::ResultCode>(
                std::bind(static_cast<F>(&cdb::AccountManager::updateAccount),
                    m_accountManager.get(), authzInfo, accountData, _1));
        ASSERT_EQ(api::ResultCode::ok, resultCode);
    }

    void whenResetPassword()
    {
        using namespace std::placeholders;

        using ResetPasswordFunc = void(cdb::AccountManager::*)(
            const AuthorizationInfo&,
            data::AccountEmail,
            std::function<void(api::ResultCode, data::AccountConfirmationCode)>);

        nx::utils::stree::ResourceContainer authorizationResources;
        authorizationResources.put(attr::secureSource, true);
        AuthorizationInfo authzInfo(std::move(authorizationResources));

        data::AccountEmail accountEmail;
        accountEmail.email = m_account.email;

        const std::tuple<api::ResultCode, data::AccountConfirmationCode> result =
            makeSyncCall<api::ResultCode, data::AccountConfirmationCode>(
                std::bind(static_cast<ResetPasswordFunc>(&cdb::AccountManager::resetPassword),
                    m_accountManager.get(), authzInfo, accountEmail, _1));
        ASSERT_EQ(api::ResultCode::ok, std::get<0>(result));
    }

    void thenExtensionHasBeenInvoked()
    {
        m_extensionCalls.pop();
    }

    template<typename Duration>
    bool equalWithError(
        std::chrono::system_clock::time_point expected,
        std::chrono::system_clock::time_point actual,
        Duration error)
    {
        return (expected >= actual - error) && (expected <= actual + error);
    }

    void thenProperTemporaryCodeIsCreated()
    {
        ASSERT_EQ(1U, m_tempPasswordManager.registeredCredentials().size());
        const auto& tempCredentials = m_tempPasswordManager.registeredCredentials()[0];
        ASSERT_EQ(1, tempCredentials.maxUseCount);

        auto curTime = nx::utils::utcTime();
        ASSERT_TRUE(equalWithError(
            nx::utils::floor<std::chrono::seconds>(m_testStartTime) +
                m_settings.accountManager().passwordResetCodeExpirationTimeout,
            std::chrono::system_clock::from_time_t(tempCredentials.expirationTimestampUtc),
            curTime - m_testStartTime));
    }

private:
    dao::AccountDataObjectFactory::Function m_factoryBak;
    conf::Settings m_settings;
    StreeManager m_streeManager;
    TemporaryAccountPasswordManagerStub m_tempPasswordManager;
    TestEmailManager m_emailManager;
    std::unique_ptr<cdb::AccountManager> m_accountManager;
    nx::utils::SyncQueue<api::AccountData> m_extensionCalls;
    std::chrono::system_clock::time_point m_testStartTime;

    data::AccountData m_account;

    virtual void afterUpdatingAccountPassword(
        nx::utils::db::QueryContext* const /*queryContext*/,
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

        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, *accountConfirmationCode) =
            makeSyncCall<api::ResultCode, data::AccountConfirmationCode>(
                std::bind(&cdb::AccountManager::registerAccount, m_accountManager.get(),
                    authzInfo, accountRegistrationData, _1));
        ASSERT_EQ(api::ResultCode::ok, resultCode);
    }

    void activateAccount(const data::AccountConfirmationCode& accountConfirmationCode)
    {
        using namespace std::placeholders;

        nx::utils::stree::ResourceContainer authorizationResources;
        authorizationResources.put(attr::secureSource, true);
        AuthorizationInfo authzInfo(std::move(authorizationResources));

        api::ResultCode resultCode = api::ResultCode::ok;
        api::AccountEmail accountEmail;
        std::tie(resultCode, accountEmail) =
            makeSyncCall<api::ResultCode, api::AccountEmail>(
                std::bind(&cdb::AccountManager::activate, m_accountManager.get(),
                    authzInfo, accountConfirmationCode, _1));
        ASSERT_EQ(api::ResultCode::ok, resultCode);
    }

    void getAccount(const std::string& accountEmail, data::AccountData* account)
    {
        using namespace std::placeholders;

        nx::utils::stree::ResourceContainer authorizationResources;
        authorizationResources.put(attr::authAccountEmail, QString::fromStdString(accountEmail));
        AuthorizationInfo authzInfo(std::move(authorizationResources));

        api::ResultCode resultCode = api::ResultCode::ok;
        std::tie(resultCode, *account) =
            makeSyncCall<api::ResultCode, api::AccountData>(
                std::bind(&cdb::AccountManager::getAccount, m_accountManager.get(),
                    authzInfo, _1));
        ASSERT_EQ(api::ResultCode::ok, resultCode);
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
} // namespace cdb
} // namespace nx
