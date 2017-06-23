#pragma once

#include <functional>
#include <string>
#include <vector>

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/network/buffer.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/counter.h>
#include <utils/db/async_sql_query_executor.h>
#include <utils/db/filter.h>

#include "cache.h"
#include "extension_pool.h"
#include "managers_types.h"
#include "notification.h"
#include "../access_control/abstract_authentication_data_provider.h"
#include "../access_control/auth_types.h"
#include "../dao/account_data_object.h"
#include "../data/account_data.h"

namespace nx {
namespace cdb {

class AbstractTemporaryAccountPasswordManager;
class AbstractEmailManager;
class StreeManager;

namespace conf { class Settings; }

class AbstractAccountManagerExtension
{
public:
    virtual ~AbstractAccountManagerExtension() = default;

    /**
     * @throw nx::db::Exception.
     */
    virtual void afterUpdatingAccountPassword(
        nx::db::QueryContext* const /*queryContext*/,
        const api::AccountData& /*account*/) {}
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
};

/**
 * NOTE: Methods of this class are re-enterable
 */
class AccountManager:
    public AbstractAccountManager,
    public AbstractAuthenticationDataProvider
{
public:
    typedef nx::utils::MoveOnlyFunc<
        nx::db::DBResult(nx::db::QueryContext*, const data::AccountUpdateDataWithEmail&)
    > UpdateAccountSubroutine;

    /**
     * @throw std::runtime_error In case of failure to pre-fill data cache
     */
    AccountManager(
        const conf::Settings& settings,
        const StreeManager& streeManager,
        AbstractTemporaryAccountPasswordManager* const tempPasswordManager,
        nx::db::AsyncSqlQueryExecutor* const dbManager,
        AbstractEmailManager* const emailManager) noexcept(false);
    virtual ~AccountManager();

    virtual void authenticateByName(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const nx::utils::stree::AbstractResourceReader& authSearchInputData,
        nx::utils::stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    //---------------------------------------------------------------------------------------------
    // Public API methods

    /**
     * Adds account in "not activated" state and sends verification email to the email address provided.
     */
    void registerAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountRegistrationData accountData,
        std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler );
    /**
     * On success, account moved to "activated" state.
     */
    void activate(
        const AuthorizationInfo& authzInfo,
        data::AccountConfirmationCode emailVerificationCode,
        std::function<void(api::ResultCode, api::AccountEmail)> completionHandler );
    
    /**
     * Retrieves account corresponding to authorization data \a authzInfo.
     */
    void getAccount(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::ResultCode, data::AccountData)> completionHandler);
    void updateAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountUpdateData accountData,
        std::function<void(api::ResultCode)> completionHandler);
    void resetPassword(
        const AuthorizationInfo& authzInfo,
        data::AccountEmail accountEmail,
        std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler);
    void reactivateAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountEmail accountEmail,
        std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler);

    void createTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryCredentialsParams params,
        std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler);

    //---------------------------------------------------------------------------------------------

    std::string generateNewAccountId() const;

    /**
     * Fetches account from cache.
     */
    virtual boost::optional<data::AccountData> findAccountByUserName(
        const std::string& userName) const override;
    
    nx::db::DBResult insertAccount(
        nx::db::QueryContext* const queryContext,
        data::AccountData account);
    nx::db::DBResult updateAccount(
        nx::db::QueryContext* const queryContext,
        data::AccountData account);

    nx::db::DBResult fetchAccountByEmail(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        data::AccountData* const accountData);

    nx::db::DBResult createPasswordResetCode(
        nx::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::chrono::seconds codeExpirationTimeout,
        data::AccountConfirmationCode* const confirmationCode);

    virtual void addExtension(AbstractAccountManagerExtension*) override;
    virtual void removeExtension(AbstractAccountManagerExtension*) override;
    // TODO: #ak Modify to an abstract extension.
    void setUpdateAccountSubroutine(UpdateAccountSubroutine func);

private:
    const conf::Settings& m_settings;
    const StreeManager& m_streeManager;
    AbstractTemporaryAccountPasswordManager* const m_tempPasswordManager;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    AbstractEmailManager* const m_emailManager;
    /** map<email, account>. */
    Cache<std::string, data::AccountData> m_cache;
    mutable QnMutex m_mutex;
    /** map<email, temporary password>. */
    std::multimap<std::string, data::TemporaryAccountCredentials> m_accountPassword;
    nx::utils::Counter m_startedAsyncCallsCounter;
    UpdateAccountSubroutine m_updateAccountSubroutine;
    std::unique_ptr<dao::AbstractAccountDataObject> m_dao;
    ExtensionPool<AbstractAccountManagerExtension> m_extensions;

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchAccounts(nx::db::QueryContext* queryContext);

    nx::db::DBResult registerNewAccountInDb(
        nx::db::QueryContext* const queryContext,
        const data::AccountData& accountData,
        data::AccountConfirmationCode* const confirmationCode);
    nx::db::DBResult issueAccountActivationCode(
        nx::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::unique_ptr<AbstractActivateAccountNotification> notification,
        data::AccountConfirmationCode* const resultData);
    void accountReactivated(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        bool requestSourceSecured,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult resultCode,
        std::string email,
        data::AccountConfirmationCode resultData,
        std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler);

    nx::db::DBResult verifyAccount(
        nx::db::QueryContext* const tran,
        const data::AccountConfirmationCode& verificationCode,
        std::string* const accountEmail);
    void sendActivateAccountResponse(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult resultCode,
        data::AccountConfirmationCode verificationCode,
        std::string accountEmail,
        std::function<void(api::ResultCode, api::AccountEmail)> completionHandler);
    void activateAccountInCache(
        std::string accountEmail,
        std::chrono::system_clock::time_point activationTime);

    nx::db::DBResult updateAccountInDb(
        bool activateAccountIfNotActive,
        nx::db::QueryContext* const tran,
        const data::AccountUpdateDataWithEmail& accountData);
    bool isValidInput(const data::AccountUpdateDataWithEmail& accountData) const;
    void updateAccountCache(
        bool activateAccountIfNotActive,
        data::AccountUpdateDataWithEmail accountData);

    nx::db::DBResult resetPassword(
        nx::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        data::AccountConfirmationCode* const confirmationCode);

    void temporaryCredentialsSaved(
        nx::utils::Counter::ScopedIncrement asyncCallLocker,
        api::ResultCode resultCode,
        const std::string& accountEmail,
        api::TemporaryCredentials temporaryCredentials,
        std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler);
};

} // namespace cdb
} // namespace nx
