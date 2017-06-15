#include <gtest/gtest.h>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/sync_call.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/cdb/dao/account_data_object.h>
#include <nx/cloud/cdb/dao/memory/dao_memory_account_data_object.h>
#include <nx/cloud/cdb/managers/account_manager.h>
#include <nx/cloud/cdb/managers/temporary_account_password_manager.h>
#include <nx/cloud/cdb/stree/stree_manager.h>
#include <nx/cloud/cdb/test_support/business_data_generator.h>

#include "base_persistent_data_test.h"
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
        m_tempPasswordManager(
            m_settings,
            &persistentDbManager()->queryExecutor()),
        m_emailManager(nullptr)
    {
        m_factoryBak = dao::AccountDataObjectFactory::instance().setCustomFunc(
            []() { return std::make_unique<dao::memory::AccountDataObject>(); });

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
                    m_accountManager.get(), authzInfo, accountData, std::placeholders::_1));
        ASSERT_EQ(api::ResultCode::ok, resultCode);
    }

    void thenExtensionHasBeenInvoked()
    {
        m_extensionCalls.pop();
    }

private:
    dao::AccountDataObjectFactory::Function m_factoryBak;
    conf::Settings m_settings;
    StreeManager m_streeManager;
    TemporaryAccountPasswordManager m_tempPasswordManager;
    TestEmailManager m_emailManager;
    std::unique_ptr<cdb::AccountManager> m_accountManager;
    nx::utils::SyncQueue<api::AccountData> m_extensionCalls;

    data::AccountData m_account;

    virtual void afterUpdatingAccountPassword(
        nx::db::QueryContext* const /*queryContext*/,
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
        accountRegistrationData.passwordHa1 = nx::utils::generateRandomName(22);

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

} // namespace test
} // namespace cdb
} // namespace nx
