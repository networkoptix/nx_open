#pragma once

#include <functional>
#include <string>
#include <vector>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <nx/network/buffer.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/counter.h>
#include <utils/db/async_sql_query_executor.h>

#include "access_control/auth_types.h"
#include "access_control/abstract_authentication_data_provider.h"
#include "cache.h"
#include "data/account_data.h"
#include "managers_types.h"
#include "notification.h"

namespace nx {
namespace cdb {

class TemporaryAccountPasswordManager;
class AbstractEmailManager;
namespace conf
{
    class Settings;
} // namespace conf
class StreeManager;

/**
 * \note Methods of this class are re-enterable
 */
class AccountManager
:
    public AbstractAuthenticationDataProvider
{
public:
    /**
     * @throw std::runtime_error In case of failure to pre-fill data cache
     */
    AccountManager(
        const conf::Settings& settings,
        const StreeManager& streeManager,
        TemporaryAccountPasswordManager* const tempPasswordManager,
        nx::db::AsyncSqlQueryExecutor* const dbManager,
        AbstractEmailManager* const emailManager) throw(std::runtime_error);
    virtual ~AccountManager();

    virtual void authenticateByName(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const stree::AbstractResourceReader& authSearchInputData,
        stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    //-----------------------------------------------------------
    // Public API methods

    /**
     * Adds account in "not activated" state and sends verification email to the email address provided.
     */
    void addAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountData accountData,
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

    //-----------------------------------------------------------

    std::string generateNewAccountId() const;

    boost::optional<data::AccountData> findAccountByUserName(
        const std::string& userName) const;
    
    nx::db::DBResult insertAccount(
        nx::db::QueryContext* const tran,
        data::AccountData accountData);

    nx::db::DBResult fetchExistingAccountByEmail(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        data::AccountData* const accountData);

    nx::db::DBResult createPasswordResetCode(
        nx::db::QueryContext* const tran,
        const std::string& accountEmail,
        data::AccountConfirmationCode* const confirmationCode);

private:
    const conf::Settings& m_settings;
    const StreeManager& m_streeManager;
    TemporaryAccountPasswordManager* const m_tempPasswordManager;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    AbstractEmailManager* const m_emailManager;
    /** map<email, account>. */
    Cache<std::string, data::AccountData> m_cache;
    mutable QnMutex m_mutex;
    /** map<email, temporary password>. */
    std::multimap<std::string, data::TemporaryAccountCredentials> m_accountPassword;
    QnCounter m_startedAsyncCallsCounter;

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchAccounts(nx::db::QueryContext* queryContext, int* const dummyResult);

    //-----------------------------------------------------
    // add_account DB operations
    nx::db::DBResult registerNewAccountInDb(
        nx::db::QueryContext* const queryContext,
        const data::AccountData& accountData,
        data::AccountConfirmationCode* const confirmationCode);
    nx::db::DBResult issueAccountActivationCode(
        nx::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::unique_ptr<AbstractActivateAccountNotification> notification,
        data::AccountConfirmationCode* const resultData);
    void accountAdded(
        QnCounter::ScopedIncrement asyncCallLocker,
        bool requestSourceSecured,
        nx::db::DBResult resultCode,
        data::AccountData accountData,
        data::AccountConfirmationCode resultData,
        std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler);
    void accountReactivated(
        QnCounter::ScopedIncrement asyncCallLocker,
        bool requestSourceSecured,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult resultCode,
        std::string email,
        data::AccountConfirmationCode resultData,
        std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler);

    //-----------------------------------------------------
    // verify_account DB operations
    nx::db::DBResult verifyAccount(
        nx::db::QueryContext* const tran,
        const data::AccountConfirmationCode& verificationCode,
        std::string* const accountEmail);
    void accountVerified(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult resultCode,
        data::AccountConfirmationCode verificationCode,
        const std::string accountEmail,
        std::function<void(api::ResultCode, api::AccountEmail)> completionHandler);

    nx::db::DBResult updateAccountInDB(
        bool activateAccountIfNotActive,
        nx::db::QueryContext* const tran,
        const data::AccountUpdateDataWithEmail& accountData);
    void accountUpdated(
        QnCounter::ScopedIncrement asyncCallLocker,
        bool authenticatedByEmailCode,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult resultCode,
        data::AccountUpdateDataWithEmail accountData,
        std::function<void(api::ResultCode)> completionHandler);

    void passwordResetCodeGenerated(
        bool hasRequestCameFromSecureSource,
        nx::db::DBResult resultCode,
        std::string accountEmail,
        data::AccountConfirmationCode confirmationCode,
        std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler);
    void temporaryCredentialsSaved(
        QnCounter::ScopedIncrement asyncCallLocker,
        api::ResultCode resultCode,
        const std::string& accountEmail,
        api::TemporaryCredentials temporaryCredentials,
        std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler);
};

} // namespace cdb
} // namespace nx
