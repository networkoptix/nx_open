#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/network/buffer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/counter.h>
#include <nx/sql/async_sql_query_executor.h>
#include <nx/sql/filter.h>

#include "cache.h"
#include "extension_pool.h"
#include "managers_types.h"
#include "notification.h"
#include "../access_control/abstract_authentication_data_provider.h"
#include "../access_control/auth_types.h"
#include "../dao/account_data_object.h"
#include "../data/account_data.h"

namespace nx::cloud::db {

class AbstractTemporaryAccountPasswordManager;
class AbstractEmailManager;
class StreeManager;

namespace conf { class Settings; }

class AbstractAccountManagerExtension
{
public:
    virtual ~AbstractAccountManagerExtension() = default;

    /**
     * @throw nx::sql::Exception.
     */
    virtual void afterUpdatingAccountPassword(
        nx::sql::QueryContext* const /*queryContext*/,
        const api::AccountData& /*account*/) {}

    virtual void afterUpdatingAccount(
        nx::sql::QueryContext*,
        const data::AccountUpdateDataWithEmail&) {}
};

/**
 * This interface introduced for testing purposes.
 * It is incomplete and quite raw.
 */
class AbstractAccountManager
{
public:
    virtual ~AbstractAccountManager() = default;

    /**
     * Fetches account from cache.
     */
    virtual boost::optional<data::AccountData> findAccountByUserName(
        const std::string& userName) const = 0;

    virtual void addExtension(AbstractAccountManagerExtension*) = 0;

    virtual void removeExtension(AbstractAccountManagerExtension*) = 0;

    virtual std::string generateNewAccountId() const = 0;

    virtual nx::sql::DBResult insertAccount(
        nx::sql::QueryContext* const queryContext,
        data::AccountData account) = 0;

    virtual nx::sql::DBResult fetchAccountByEmail(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail,
        data::AccountData* const accountData) = 0;

    virtual nx::sql::DBResult createPasswordResetCode(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::chrono::seconds codeExpirationTimeout,
        data::AccountConfirmationCode* const confirmationCode) = 0;
};

/**
 * NOTE: Methods of this class are re-enterable
 */
class AccountManager:
    public AbstractAccountManager,
    public AbstractAuthenticationDataProvider
{
public:
    /**
     * @throw std::runtime_error In case of failure to pre-fill data cache
     */
    AccountManager(
        const conf::Settings& settings,
        const StreeManager& streeManager,
        AbstractTemporaryAccountPasswordManager* const tempPasswordManager,
        nx::sql::AsyncSqlQueryExecutor* const dbManager,
        AbstractEmailManager* const emailManager) noexcept(false);
    virtual ~AccountManager();

    virtual void authenticateByName(
        const nx::network::http::StringType& username,
        const std::function<bool(const nx::Buffer&)>& validateHa1Func,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::Result)> completionHandler) override;

    //---------------------------------------------------------------------------------------------
    // Public API methods

    /**
     * Adds account in "not activated" state and sends verification email to the email address provided.
     */
    void registerAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountRegistrationData accountData,
        std::function<void(api::Result, data::AccountConfirmationCode)> completionHandler );
    /**
     * On success, account moved to "activated" state.
     */
    void activate(
        const AuthorizationInfo& authzInfo,
        data::AccountConfirmationCode emailVerificationCode,
        std::function<void(api::Result, api::AccountEmail)> completionHandler );

    /**
     * Retrieves account corresponding to authorization data \a authzInfo.
     */
    void getAccount(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::Result, data::AccountData)> completionHandler);

    void updateAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountUpdateData accountData,
        std::function<void(api::Result)> completionHandler);

    void resetPassword(
        const AuthorizationInfo& authzInfo,
        data::AccountEmail accountEmail,
        std::function<void(api::Result, data::AccountConfirmationCode)> completionHandler);

    void reactivateAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountEmail accountEmail,
        std::function<void(api::Result, data::AccountConfirmationCode)> completionHandler);

    void createTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryCredentialsParams params,
        std::function<void(api::Result, api::TemporaryCredentials)> completionHandler);

    //---------------------------------------------------------------------------------------------

    virtual std::string generateNewAccountId() const override;

    /**
     * Fetches account from cache.
     */
    virtual boost::optional<data::AccountData> findAccountByUserName(
        const std::string& userName) const override;

    virtual nx::sql::DBResult insertAccount(
        nx::sql::QueryContext* const queryContext,
        data::AccountData account) override;
    nx::sql::DBResult updateAccount(
        nx::sql::QueryContext* const queryContext,
        data::AccountData account);

    virtual nx::sql::DBResult fetchAccountByEmail(
        nx::sql::QueryContext* queryContext,
        const std::string& accountEmail,
        data::AccountData* const accountData) override;

    virtual nx::sql::DBResult createPasswordResetCode(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::chrono::seconds codeExpirationTimeout,
        data::AccountConfirmationCode* const confirmationCode) override;

    virtual void addExtension(AbstractAccountManagerExtension*) override;
    virtual void removeExtension(AbstractAccountManagerExtension*) override;

private:
    const conf::Settings& m_settings;
    const StreeManager& m_streeManager;
    AbstractTemporaryAccountPasswordManager* const m_tempPasswordManager;
    nx::sql::AsyncSqlQueryExecutor* const m_dbManager;
    AbstractEmailManager* const m_emailManager;
    /** map<email, account>. */
    Cache<std::string, data::AccountData> m_cache;
    mutable QnMutex m_mutex;
    /** map<email, temporary password>. */
    std::multimap<std::string, data::TemporaryAccountCredentials> m_accountPassword;
    nx::utils::Counter m_startedAsyncCallsCounter;
    std::unique_ptr<dao::AbstractAccountDataObject> m_dao;
    ExtensionPool<AbstractAccountManagerExtension> m_extensions;

    nx::sql::DBResult fillCache();
    nx::sql::DBResult fetchAccounts(nx::sql::QueryContext* queryContext);

    nx::sql::DBResult registerNewAccountInDb(
        nx::sql::QueryContext* const queryContext,
        const data::AccountData& accountData,
        data::AccountConfirmationCode* const confirmationCode);

    nx::sql::DBResult issueAccountActivationCode(
        nx::sql::QueryContext* const queryContext,
        const data::AccountData& account,
        std::unique_ptr<AbstractActivateAccountNotification> notification,
        data::AccountConfirmationCode* const resultData);

    std::string generateAccountActivationCode(
        nx::sql::QueryContext* const queryContext,
        const std::string& email,
        const std::chrono::seconds& codeExpirationTime);

    void accountReactivated(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        bool requestSourceSecured,
        nx::sql::DBResult resultCode,
        std::string email,
        data::AccountConfirmationCode resultData,
        std::function<void(api::Result, data::AccountConfirmationCode)> completionHandler);

    nx::sql::DBResult verifyAccount(
        nx::sql::QueryContext* const tran,
        const data::AccountConfirmationCode& verificationCode,
        std::string* const accountEmail);

    void sendActivateAccountResponse(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::sql::DBResult resultCode,
        data::AccountConfirmationCode verificationCode,
        std::string accountEmail,
        std::function<void(api::Result, api::AccountEmail)> completionHandler);

    void activateAccountInCache(
        std::string accountEmail,
        std::chrono::system_clock::time_point activationTime);

    nx::sql::DBResult updateAccountInDb(
        bool activateAccountIfNotActive,
        nx::sql::QueryContext* const tran,
        const data::AccountUpdateDataWithEmail& accountData);
    bool isValidInput(const data::AccountUpdateDataWithEmail& accountData) const;
    void updateAccountCache(data::AccountUpdateDataWithEmail accountData);

    nx::sql::DBResult issueRestorePasswordCode(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountEmail,
        data::AccountConfirmationCode* const confirmationCode);

    void temporaryCredentialsSaved(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        api::Result result,
        const std::string& accountEmail,
        api::TemporaryCredentials temporaryCredentials,
        std::function<void(api::Result, api::TemporaryCredentials)> completionHandler);
};

} // namespace nx::cloud::db
