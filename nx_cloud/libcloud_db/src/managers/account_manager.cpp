#include "account_manager.h"

#include <chrono>
#include <functional>
#include <iostream>

#include <QtCore/QDateTime>
#include <QtSql/QSqlQuery>

#include <cloud_db_client/src/cdb_request_path.h>
#include <cloud_db_client/src/data/types.h>
#include <nx/email/mustache/mustache_helper.h>
#include <nx/fusion/serialization/lexical.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/utils/log/log.h>
#include <nx/utils/time.h>
#include <utils/common/app_info.h>
#include <utils/common/guard.h>

#include "access_control/authentication_manager.h"
#include "email_manager.h"
#include "notification.h"
#include "settings.h"
#include "stree/cdb_ns.h"
#include "stree/stree_manager.h"
#include "temporary_account_password_manager.h"

namespace nx {
namespace cdb {

static const std::chrono::seconds kUnconfirmedAccountExpirationSec(3*24*60*60);

AccountManager::AccountManager(
    const conf::Settings& settings,
    const StreeManager& streeManager,
    TemporaryAccountPasswordManager* const tempPasswordManager,
    nx::db::AsyncSqlQueryExecutor* const dbManager,
    AbstractEmailManager* const emailManager) throw(std::runtime_error)
:
    m_settings(settings),
    m_streeManager(streeManager),
    m_tempPasswordManager(tempPasswordManager),
    m_dbManager(dbManager),
    m_emailManager(emailManager)
{
    if( fillCache() != db::DBResult::ok )
        throw std::runtime_error( "Failed to fill account cache" );
}

AccountManager::~AccountManager()
{
    //assuming no public methods of this class can be called anymore,
    //  but we have to wait for all scheduled async calls to finish
    m_startedAsyncCallsCounter.wait();
}

void AccountManager::authenticateByName(
    const nx_http::StringType& username,
    std::function<bool(const nx::Buffer&)> validateHa1Func,
    const stree::AbstractResourceReader& authSearchInputData,
    stree::ResourceContainer* const authProperties,
    nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    {
        QnMutexLocker lk(&m_mutex);

        auto account = m_cache.find(toStdString(username));
        if (account && validateHa1Func(account->passwordHa1.c_str()))
        {
            authProperties->put(
                cdb::attr::authAccountEmail,
                username);
            completionHandler(api::ResultCode::ok);
            return;
        }
    }

    //NOTE currently, all authenticateByName are sync.
    //  Changing it to async requires some HTTP authentication refactoring

    //authenticating by temporary password
    m_tempPasswordManager->authenticateByName(
        username,
        std::move(validateHa1Func),
        authSearchInputData,
        authProperties,
        [username, authProperties, completionHandler = std::move(completionHandler), this](
            api::ResultCode authResult) mutable
        {
            if (authResult == api::ResultCode::ok)
            {
                bool authenticatedByEmailCode = false;
                authProperties->get(
                    attr::authenticatedByEmailCode,
                    &authenticatedByEmailCode);
                if (authenticatedByEmailCode)
                {
                    //activating account
                    m_cache.atomicUpdate(
                        std::string(username.constData(), username.size()),
                        [](api::AccountData& account) {
                            account.statusCode = api::AccountStatus::activated;
                        });
                }
            }
            completionHandler(authResult);
        });
}

void AccountManager::registerAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountData account,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{
    account.statusCode = api::AccountStatus::awaitingActivation;

    //fetching request source
    bool requestSourceSecured = false;
    authzInfo.get(attr::secureSource, &requestSourceSecured);

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountData, data::AccountConfirmationCode>(
        std::bind(&AccountManager::registerNewAccountInDb, this, _1, _2, _3),
        std::move(account),
        [locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler),
            requestSourceSecured](
                nx::db::QueryContext* const /*queryContext*/,
                db::DBResult dbResult,
                data::AccountData /*account*/,
                data::AccountConfirmationCode confirmationCode)
        {
            completionHandler(
                dbResultToApiResult(dbResult),
                requestSourceSecured
                    ? std::move(confirmationCode)
                    : data::AccountConfirmationCode());
        });
}

void AccountManager::activate(
    const AuthorizationInfo& /*authzInfo*/,
    data::AccountConfirmationCode emailVerificationCode,
    std::function<void(api::ResultCode, api::AccountEmail)> completionHandler )
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountConfirmationCode, std::string>(
        std::bind(&AccountManager::verifyAccount, this, _1, _2, _3),
        std::move(emailVerificationCode),
        std::bind(&AccountManager::accountVerified, this,
                    m_startedAsyncCallsCounter.getScopedIncrement(),
                    _1, _2, _3, _4, std::move(completionHandler)));
}

void AccountManager::getAccount(
    const AuthorizationInfo& authzInfo,
    std::function<void(api::ResultCode, data::AccountData)> completionHandler )
{
    QString accountEmail;
    if (!authzInfo.get( attr::authAccountEmail, &accountEmail))
    {
        completionHandler(api::ResultCode::forbidden, data::AccountData());
        return;
    }

    if (auto accountData = m_cache.find(accountEmail.toStdString()))
    {
        accountData.get().passwordHa1.clear();
        completionHandler(api::ResultCode::ok, std::move(accountData.get()));
        return;
    }

    // Very strange. Account has been authenticated, but not found in cache.
    completionHandler(api::ResultCode::notFound, data::AccountData());
}

void AccountManager::updateAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountUpdateData accountData,
    std::function<void(api::ResultCode)> completionHandler)
{
    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
    {
        completionHandler(api::ResultCode::forbidden);
        return;
    }
    bool authenticatedByEmailCode = false;
    authzInfo.get(attr::authenticatedByEmailCode, &authenticatedByEmailCode);

    data::AccountUpdateDataWithEmail updateDataWithEmail(std::move(accountData));
    updateDataWithEmail.email = accountEmail;

    auto onUpdateCompletion = 
        [this, 
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            authenticatedByEmailCode,
            completionHandler = std::move(completionHandler)](
                nx::db::QueryContext* /*queryContext*/,
                nx::db::DBResult resultCode,
                data::AccountUpdateDataWithEmail accountData)
        {
            if (resultCode == db::DBResult::ok)
                updateAccountCache(authenticatedByEmailCode, std::move(accountData));
            completionHandler(dbResultToApiResult(resultCode));
        };

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountUpdateDataWithEmail>(
        std::bind(&AccountManager::updateAccountInDB, this, authenticatedByEmailCode, _1, _2),
        std::move(updateDataWithEmail),
        std::move(onUpdateCompletion));
}

void AccountManager::resetPassword(
    const AuthorizationInfo& authzInfo,
    data::AccountEmail accountEmail,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{
    auto account = m_cache.find(accountEmail.email);
    if (!account)
    {
        NX_LOG(lm("%1 (%2). Account not found").
            arg(kAccountPasswordResetPath).arg(accountEmail.email), cl_logDEBUG1);
        return completionHandler(
            api::ResultCode::notFound,
            data::AccountConfirmationCode());
    }

    //fetching request source
    bool hasRequestCameFromSecureSource = false;
    authzInfo.get(attr::secureSource, &hasRequestCameFromSecureSource);

    m_dbManager->executeUpdate<std::string, data::AccountConfirmationCode>(
        [this](
            nx::db::QueryContext* const queryContext,
            const std::string& accountEmail,
            data::AccountConfirmationCode* const confirmationCode)
        {
            return resetPassword(queryContext, accountEmail, confirmationCode);
        },
        accountEmail.email,
        [this, hasRequestCameFromSecureSource,
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::db::QueryContext* /*queryContext*/,
                nx::db::DBResult dbResultCode,
                std::string /*accountEmail*/,
                data::AccountConfirmationCode confirmationCode)
        {
            return completionHandler(
                dbResultToApiResult(dbResultCode),
                hasRequestCameFromSecureSource
                    ? std::move(confirmationCode)
                    : data::AccountConfirmationCode());
        });
}

void AccountManager::reactivateAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountEmail accountEmail,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{
    const auto existingAccount = m_cache.find(accountEmail.email);
    if (!existingAccount)
    {
        NX_LOG(lm("/account/reactivate. Unknown account %1").
            arg(accountEmail.email), cl_logDEBUG1);
        return completionHandler(
            api::ResultCode::notFound,
            data::AccountConfirmationCode());
    }

    if (existingAccount->statusCode == api::AccountStatus::activated)
    {
        NX_LOG(lm("/account/reactivate. Not reactivating of already activated account %1").
            arg(accountEmail.email), cl_logDEBUG1);
        return completionHandler(
            api::ResultCode::forbidden,
            data::AccountConfirmationCode());
    }

    //fetching request source
    bool requestSourceSecured = false;
    authzInfo.get(attr::secureSource, &requestSourceSecured);

    using namespace std::placeholders;
    m_dbManager->executeUpdate<std::string, data::AccountConfirmationCode>(
        [this](
            nx::db::QueryContext* const queryContext,
            const std::string& accountEmail,
            data::AccountConfirmationCode* const resultData)
        {
            return issueAccountActivationCode(
                queryContext,
                accountEmail, 
                std::make_unique<ActivateAccountNotification>(),
                resultData);
        },
        std::move(accountEmail.email),
        std::bind(&AccountManager::accountReactivated, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            requestSourceSecured,
            _1, _2, _3, _4, std::move(completionHandler)));
}

void AccountManager::createTemporaryCredentials(
    const AuthorizationInfo& authzInfo,
    data::TemporaryCredentialsParams params,
    std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler)
{
    std::string accountEmail;
    if (!authzInfo.get(attr::authAccountEmail, &accountEmail))
    {
        NX_ASSERT(false);
        return completionHandler(
            api::ResultCode::forbidden,
            api::TemporaryCredentials());
    }

    auto account = m_cache.find(accountEmail);
    if (!account)
    {
        //this can happen due to some race
        NX_LOG(lm("%1 (%2). Account not found").
            arg(kAccountCreateTemporaryCredentialsPath).arg(accountEmail), cl_logDEBUG1);
        return completionHandler(
            api::ResultCode::notFound,
            api::TemporaryCredentials());
    }

    if (!params.type.empty())
    {
        //fetching timeouts by type
        params.timeouts = api::TemporaryCredentialsTimeouts();
        m_streeManager.search(
            StreeOperation::getTemporaryCredentialsParameters,
            stree::SingleResourceContainer(
                attr::credentialsType, QString::fromStdString(params.type)),
            &params);
        if (params.timeouts.expirationPeriod == std::chrono::seconds::zero())
            return completionHandler(
                api::ResultCode::badRequest,
                api::TemporaryCredentials());
    }

    //generating temporary password
    data::TemporaryAccountCredentials tempPasswordData;
    //temporary login is generated by \a addRandomCredentials
    tempPasswordData.accountEmail = accountEmail;
    tempPasswordData.realm = AuthenticationManager::realm().constData();
    tempPasswordData.expirationTimestampUtc =
        nx::utils::timeSinceEpoch().count() +
        params.timeouts.expirationPeriod.count();
    if (params.timeouts.autoProlongationEnabled)
        tempPasswordData.prolongationPeriodSec = 
            std::chrono::duration_cast<std::chrono::seconds>(
                params.timeouts.prolongationPeriod).count();
    tempPasswordData.maxUseCount = std::numeric_limits<int>::max();
    //filling in access rights
    tempPasswordData.accessRights.requestsDenied.push_back(kAccountUpdatePath);
    tempPasswordData.accessRights.requestsDenied.push_back(kAccountPasswordResetPath);
    m_tempPasswordManager->addRandomCredentials(&tempPasswordData);

    api::TemporaryCredentials temporaryCredentials;
    temporaryCredentials.login = tempPasswordData.login;
    temporaryCredentials.password = tempPasswordData.password;
    temporaryCredentials.timeouts = params.timeouts;

    //adding temporary password
    m_tempPasswordManager->registerTemporaryCredentials(
        authzInfo,
        std::move(tempPasswordData),
        std::bind(&AccountManager::temporaryCredentialsSaved, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            std::placeholders::_1,
            std::move(accountEmail),
            std::move(temporaryCredentials),
            std::move(completionHandler)));
}

std::string AccountManager::generateNewAccountId() const
{
    return QnUuid::createUuid().toSimpleString().toStdString();
}

boost::optional<data::AccountData> AccountManager::findAccountByUserName(
    const std::string& userName ) const
{
    return m_cache.find(userName);
}

db::DBResult AccountManager::insertAccount(
    nx::db::QueryContext* const queryContext,
    data::AccountData account)
{
    if (account.id.empty())
        account.id = generateNewAccountId();
    if (account.customization.empty())
        account.customization = QnAppInfo::customizationName().toStdString();

    QSqlQuery insertAccountQuery(*queryContext->connection());
    insertAccountQuery.prepare(
        R"sql(
        INSERT INTO account (id, email, password_ha1, password_ha1_sha256, full_name, customization, status_code)
        VALUES  (:id, :email, :passwordHa1, :passwordHa1Sha256, :fullName, :customization, :statusCode)
        )sql");
    QnSql::bind(account, &insertAccountQuery);
    if (!insertAccountQuery.exec())
    {
        NX_LOG(lm("Could not insert account (%1, %2) into DB. %3")
            .arg(account.email).arg(QnLexical::serialized(account.statusCode))
            .arg(insertAccountQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, account = std::move(account)]()
        {
            auto email = account.email;
            m_cache.insert(std::move(email), std::move(account));
        });

    return nx::db::DBResult::ok;
}

nx::db::DBResult AccountManager::updateAccount(
    nx::db::QueryContext* const queryContext,
    data::AccountData account)
{
    NX_ASSERT(!account.id.empty() && !account.email.empty());
    if (account.id.empty() || account.email.empty())
        return nx::db::DBResult::ioError;

    QSqlQuery updateAccountQuery(*queryContext->connection());
    updateAccountQuery.prepare(
        R"sql(
        UPDATE account 
        SET password_ha1=:passwordHa1, password_ha1_sha256=:passwordHa1Sha256, 
            full_name=:fullName, customization=:customization, status_code=:statusCode
        WHERE id=:id AND email=:email
        )sql");
    QnSql::bind(account, &updateAccountQuery);
    if (!updateAccountQuery.exec())
    {
        NX_LOG(lm("Could not update account (%1, %2) into DB. %3")
            .arg(account.email).arg(QnLexical::serialized(account.statusCode))
            .arg(updateAccountQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::db::DBResult::ioError;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, account = std::move(account)]() mutable
        {
            auto email = account.email;
            m_cache.atomicUpdate(
                email,
                [newAccount = std::move(account)](data::AccountData& account) mutable
                {
                    account = std::move(newAccount);
                });
        });

    return nx::db::DBResult::ok;
}

nx::db::DBResult AccountManager::fetchAccountByEmail(
    nx::db::QueryContext* queryContext,
    const std::string& accountEmail,
    data::AccountData* const accountData)
{
    QSqlQuery fetchAccountQuery(*queryContext->connection());
    fetchAccountQuery.setForwardOnly(true);
    fetchAccountQuery.prepare(
        R"sql(
        SELECT id, email, password_ha1 as passwordHa1, password_ha1_sha256 as passwordHa1Sha256,
               full_name as fullName, customization, status_code as statusCode
        FROM account
        WHERE email=:email
        )sql");
    fetchAccountQuery.bindValue(":email", QnSql::serialized_field(accountEmail));
    if (!fetchAccountQuery.exec())
    {
        NX_LOGX(lm("Error fetching account %1 from DB. %2")
            .arg(accountEmail).arg(fetchAccountQuery.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    if (!fetchAccountQuery.next())
        return nx::db::DBResult::notFound;

    // Account exists.
    QnSql::fetch(
        QnSql::mapping<data::AccountData>(fetchAccountQuery),
        fetchAccountQuery.record(),
        accountData);
    return db::DBResult::ok;
}

nx::db::DBResult AccountManager::createPasswordResetCode(
    nx::db::QueryContext* const queryContext,
    const std::string& accountEmail,
    data::AccountConfirmationCode* const confirmationCode)
{
    //generating temporary password
    data::TemporaryAccountCredentials tempPasswordData;
    tempPasswordData.accountEmail = accountEmail;
    tempPasswordData.login = accountEmail;
    tempPasswordData.realm = AuthenticationManager::realm().constData();
    tempPasswordData.expirationTimestampUtc =
        nx::utils::timeSinceEpoch().count() +
        m_settings.accountManager().passwordResetCodeExpirationTimeout.count();
    tempPasswordData.maxUseCount = 1;
    tempPasswordData.isEmailCode = true;
    tempPasswordData.accessRights.requestsAllowed.push_back(kAccountUpdatePath);
    m_tempPasswordManager->addRandomCredentials(&tempPasswordData);

    //preparing confirmation code
    auto resetCodeStr = tempPasswordData.password + ":" + accountEmail;
    confirmationCode->code = QByteArray::fromRawData(
        resetCodeStr.data(), (int)resetCodeStr.size()).toBase64().constData();

    return m_tempPasswordManager->registerTemporaryCredentials(
        queryContext,
        std::move(tempPasswordData));
}

void AccountManager::setUpdateAccountSubroutine(UpdateAccountSubroutine func)
{
    m_updateAccountSubroutine = std::move(func);
}

db::DBResult AccountManager::fillCache()
{
    std::promise<db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    using namespace std::placeholders;
    m_dbManager->executeSelect<int>(
        std::bind(&AccountManager::fetchAccounts, this, _1, _2),
        [&cacheFilledPromise](
            nx::db::QueryContext* /*queryContext*/,
            db::DBResult dbResult,
            int /*dummyResult*/ )
        {
            cacheFilledPromise.set_value( dbResult );
        });

    //waiting for completion
    future.wait();
    return future.get();
}

db::DBResult AccountManager::fetchAccounts( 
    nx::db::QueryContext* queryContext,
    int* const /*dummyResult*/ )
{
    QSqlQuery readAccountsQuery(*queryContext->connection());
    readAccountsQuery.setForwardOnly(true);
    readAccountsQuery.prepare(
        "SELECT id, email, password_ha1 as passwordHa1, password_ha1_sha256 as passwordHa1Sha256, "
               "full_name as fullName, customization, status_code as statusCode "
        "FROM account" );
    if (!readAccountsQuery.exec())
    {
        NX_LOG(lit("Failed to read account list from DB. %1").
            arg(readAccountsQuery.lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    std::vector<data::AccountData> accounts;
    QnSql::fetch_many( readAccountsQuery, &accounts );

    for (auto& account : accounts)
    {
        auto emailCopy = account.email;
        if (!m_cache.insert(std::move(emailCopy), std::move(account)))
        {
            NX_ASSERT( false );
        }
    }

    return db::DBResult::ok;
}

nx::db::DBResult AccountManager::registerNewAccountInDb(
    nx::db::QueryContext* const queryContext,
    const data::AccountData& accountData,
    data::AccountConfirmationCode* const confirmationCode)
{
    data::AccountData existingAccount;
    auto dbResult = fetchAccountByEmail(
        queryContext,
        accountData.email,
        &existingAccount);
    if (dbResult != nx::db::DBResult::ok &&
        dbResult != nx::db::DBResult::notFound)
    {
        NX_LOGX(lm("Failed to fetch account by email %1").arg(accountData.email), cl_logDEBUG1);
        return dbResult;
    }

    if (dbResult == nx::db::DBResult::ok)
    {
        if (existingAccount.statusCode != api::AccountStatus::invited)
        {
            NX_LOG(lm("Failed to add account with already used email %1, status %2").
                arg(accountData.email).arg(QnLexical::serialized(existingAccount.statusCode)),
                cl_logDEBUG1);
            return nx::db::DBResult::uniqueConstraintViolation;
        }
        
        existingAccount.statusCode = api::AccountStatus::awaitingActivation;
        // Merging existing account with new one.
        auto accountIdBak = std::move(existingAccount.id);
        existingAccount = accountData;
        existingAccount.id = std::move(accountIdBak);

        dbResult = updateAccount(queryContext, std::move(existingAccount));
        if (dbResult != nx::db::DBResult::ok)
        {
            NX_LOGX(lm("Failed to update existing account %1")
                .arg(existingAccount.email), cl_logDEBUG1);
            return dbResult;
        }
    }
    else if (dbResult == nx::db::DBResult::notFound)
    {
        dbResult = insertAccount(queryContext, accountData);
        if (dbResult != nx::db::DBResult::ok)
        {
            NX_LOGX(lm("Failed to insert new account. Email %1")
                .arg(accountData.email), cl_logDEBUG1);
            return dbResult;
        }
    }
    else
    {
        NX_ASSERT(false);
        return nx::db::DBResult::ioError;
    }

    return issueAccountActivationCode(
        queryContext,
        accountData.email,
        std::make_unique<ActivateAccountNotification>(),
        confirmationCode);
}

db::DBResult AccountManager::issueAccountActivationCode(
    nx::db::QueryContext* const queryContext,
    const std::string& accountEmail,
    std::unique_ptr<AbstractActivateAccountNotification> notification,
    data::AccountConfirmationCode* const resultData)
{
    //removing already-existing activation codes
    QSqlQuery fetchActivationCodesQuery(*queryContext->connection());
    fetchActivationCodesQuery.setForwardOnly(true);
    fetchActivationCodesQuery.prepare(
        "SELECT verification_code "
        "FROM email_verification "
        "WHERE account_id=(SELECT id FROM account WHERE email=?)");
    fetchActivationCodesQuery.bindValue(
        0,
        QnSql::serialized_field(accountEmail));
    if (!fetchActivationCodesQuery.exec())
    {
        NX_LOG(lm("Could not fetch account %1 activation codes from DB. %2").
            arg(accountEmail).arg(fetchActivationCodesQuery.lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }
    if (fetchActivationCodesQuery.next())
    {
        //returning existing verification code
        resultData->code =
            fetchActivationCodesQuery.
                value(lit("verification_code")).toString().toStdString();
    }
    else
    {
        //inserting email verification code
        const auto emailVerificationCode = QnUuid::createUuid().toByteArray().toHex();
        QSqlQuery insertEmailVerificationQuery(*queryContext->connection());
        insertEmailVerificationQuery.prepare(
            "INSERT INTO email_verification( account_id, verification_code, expiration_date ) "
                                   "VALUES ( (SELECT id FROM account WHERE email=?), ?, ? )" );
        insertEmailVerificationQuery.bindValue(
            0,
            QnSql::serialized_field(accountEmail));
        insertEmailVerificationQuery.bindValue(
            1,
            emailVerificationCode);
        insertEmailVerificationQuery.bindValue(
            2,
            QDateTime::currentDateTimeUtc().addSecs(kUnconfirmedAccountExpirationSec.count()));
        if( !insertEmailVerificationQuery.exec() )
        {
            NX_LOG(lit("Could not insert account verification code into DB. %1").
                arg(insertEmailVerificationQuery.lastError().text()), cl_logDEBUG1);
            return db::DBResult::ioError;
        }
        resultData->code.assign(
            emailVerificationCode.constData(),
            emailVerificationCode.size());
    }

    notification->setActivationCode(resultData->code);
    notification->setAddressee(accountEmail);
    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, notification = std::move(notification)]()
        {
            m_emailManager->sendAsync(
                *notification,
                std::function<void(bool)>());
        });

    return db::DBResult::ok;
}

void AccountManager::accountReactivated(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    bool requestSourceSecured,
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult resultCode,
    std::string /*email*/,
    data::AccountConfirmationCode resultData,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{
    //adding activation code only in response to portal
    completionHandler(
        resultCode == db::DBResult::ok
            ? api::ResultCode::ok
            : api::ResultCode::dbError,
        requestSourceSecured
            ? std::move(resultData)
            : data::AccountConfirmationCode());
}

nx::db::DBResult AccountManager::verifyAccount(
    nx::db::QueryContext* const queryContext,
    const data::AccountConfirmationCode& verificationCode,
    std::string* const resultAccountEmail )
{
    QSqlQuery getAccountByVerificationCode(*queryContext->connection());
    getAccountByVerificationCode.setForwardOnly(true);
    getAccountByVerificationCode.prepare(
        "SELECT a.email "
        "FROM email_verification ev, account a "
        "WHERE ev.account_id = a.id AND ev.verification_code LIKE :code" );
    QnSql::bind( verificationCode, &getAccountByVerificationCode );
    if( !getAccountByVerificationCode.exec() )
        return db::DBResult::ioError;
    if( !getAccountByVerificationCode.next() )
    {
        NX_LOG( lit("Email verification code %1 was not found in the database").
            arg( QString::fromStdString(verificationCode.code) ), cl_logDEBUG1 );
        return db::DBResult::notFound;
    }
    const std::string accountEmail = QnSql::deserialized_field<QString>(
        getAccountByVerificationCode.value(0)).toStdString();

    QSqlQuery removeVerificationCode(*queryContext->connection());
    removeVerificationCode.prepare(
        "DELETE FROM email_verification WHERE verification_code LIKE :code" );
    QnSql::bind( verificationCode, &removeVerificationCode );
    if( !removeVerificationCode.exec() )
    {
        NX_LOG(lit("Failed to remove account verification code %1 from DB. %2")\
            .arg(QString::fromStdString(verificationCode.code))
            .arg(removeVerificationCode.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    QSqlQuery updateAccountStatus(*queryContext->connection());
    updateAccountStatus.prepare(
        "UPDATE account SET status_code = ? WHERE email = ?" );
    updateAccountStatus.bindValue( 0, static_cast<int>(api::AccountStatus::activated) );
    updateAccountStatus.bindValue( 1, QnSql::serialized_field(accountEmail) );
    if( !updateAccountStatus.exec() )
    {
        NX_LOG(lm("Failed to update account %1 status. %2").
            arg(accountEmail).arg(updateAccountStatus.lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    *resultAccountEmail = std::move(accountEmail);

    return db::DBResult::ok;
}

void AccountManager::accountVerified(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::QueryContext* /*queryContext*/,
    nx::db::DBResult resultCode,
    data::AccountConfirmationCode /*verificationCode*/,
    const std::string accountEmail,
    std::function<void(api::ResultCode, api::AccountEmail)> completionHandler )
{
    if( resultCode != db::DBResult::ok )
        return completionHandler(
            dbResultToApiResult(resultCode),
            api::AccountEmail());

    m_cache.atomicUpdate(
        accountEmail,
        []( api::AccountData& account ){ account.statusCode = api::AccountStatus::activated; } );
    api::AccountEmail response;
    response.email = accountEmail;
    completionHandler(
        dbResultToApiResult(resultCode),
        std::move(response));
}

nx::db::DBResult AccountManager::updateAccountInDB(
    bool activateAccountIfNotActive,
    nx::db::QueryContext* const queryContext,
    const data::AccountUpdateDataWithEmail& accountData)
{
    if (!isValidInput(accountData))
        return nx::db::DBResult::cancelled;

    std::vector<nx::db::SqlFilterField> fieldsToSet = 
        prepareAccountFieldsToUpdate(accountData, activateAccountIfNotActive);

    auto dbResult = executeUpdateAccountQuery(
        queryContext,
        accountData.email,
        std::move(fieldsToSet));
    if (dbResult != nx::db::DBResult::ok)
        return dbResult;

    if (accountData.passwordHa1 || accountData.passwordHa1Sha256)
    {
        dbResult = m_tempPasswordManager->removeTemporaryPasswordsFromDbByAccountEmail(
            queryContext,
            accountData.email);
        if (dbResult != nx::db::DBResult::ok)
            return dbResult;
    }

    if (m_updateAccountSubroutine)
        return m_updateAccountSubroutine(queryContext, accountData);

    return nx::db::DBResult::ok;
}

bool AccountManager::isValidInput(const data::AccountUpdateDataWithEmail& accountData) const
{
    NX_ASSERT(
        static_cast<bool>(accountData.passwordHa1) ||
        static_cast<bool>(accountData.passwordHa1Sha256) ||
        static_cast<bool>(accountData.fullName) ||
        static_cast<bool>(accountData.customization));

    if (!(accountData.passwordHa1 || accountData.passwordHa1Sha256 ||
          accountData.fullName || accountData.customization))
    {
        // Nothing to do.
        return false;
    }

    return true;
}

std::vector<nx::db::SqlFilterField> AccountManager::prepareAccountFieldsToUpdate(
    const data::AccountUpdateDataWithEmail& accountData,
    bool activateAccountIfNotActive)
{
    std::vector<nx::db::SqlFilterField> fieldsToSet;

    if (accountData.passwordHa1)
        fieldsToSet.push_back({
            "password_ha1", ":passwordHa1",
            QnSql::serialized_field(accountData.passwordHa1.get())});

    if (accountData.passwordHa1Sha256)
        fieldsToSet.push_back({
            "password_ha1_sha256", ":passwordHa1Sha256",
            QnSql::serialized_field(accountData.passwordHa1Sha256.get())});

    if (accountData.fullName)
        fieldsToSet.push_back({
            "full_name", ":fullName",
            QnSql::serialized_field(accountData.fullName.get())});

    if (accountData.customization)
        fieldsToSet.push_back({
            "customization", ":customization",
            QnSql::serialized_field(accountData.customization.get())});

    if (activateAccountIfNotActive)
        fieldsToSet.push_back({
            "status_code", ":status_code",
            QnSql::serialized_field(static_cast<int>(api::AccountStatus::activated))});

    return fieldsToSet;
}

nx::db::DBResult AccountManager::executeUpdateAccountQuery(
    nx::db::QueryContext* const queryContext,
    const std::string& accountEmail,
    std::vector<nx::db::SqlFilterField> fieldsToSet)
{
    QSqlQuery updateAccountQuery(*queryContext->connection());
    updateAccountQuery.prepare(
        lit(R"sql(
            UPDATE account SET %1 WHERE email=:email
            )sql").arg(db::joinFields(fieldsToSet, ",")));
    db::bindFields(&updateAccountQuery, fieldsToSet);
    updateAccountQuery.bindValue(
        ":email",
        QnSql::serialized_field(accountEmail));
    if (!updateAccountQuery.exec())
    {
        NX_LOG(lit("Could not update account in DB. %1").
            arg(updateAccountQuery.lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

void AccountManager::updateAccountCache(
    bool activateAccountIfNotActive,
    data::AccountUpdateDataWithEmail accountData)
{
    m_cache.atomicUpdate(
        accountData.email,
        [&accountData, activateAccountIfNotActive](api::AccountData& account)
        {
            if (accountData.passwordHa1)
                account.passwordHa1 = accountData.passwordHa1.get();
            if (accountData.passwordHa1Sha256)
                account.passwordHa1Sha256 = accountData.passwordHa1Sha256.get();
            if (accountData.fullName)
                account.fullName = accountData.fullName.get();
            if (accountData.customization)
                account.customization = accountData.customization.get();
            if (activateAccountIfNotActive)
                account.statusCode = api::AccountStatus::activated;
        });

    if (accountData.passwordHa1 || accountData.passwordHa1Sha256)
    {
        // Removing account's temporary passwords.
        m_tempPasswordManager->
            removeTemporaryPasswordsFromCacheByAccountEmail(
                accountData.email);
    }
}

nx::db::DBResult AccountManager::resetPassword(
    nx::db::QueryContext* const queryContext,
    const std::string& accountEmail,
    data::AccountConfirmationCode* const confirmationCode)
{
    const auto dbResult = createPasswordResetCode(
        queryContext, accountEmail, confirmationCode);
    if (dbResult != nx::db::DBResult::ok)
    {
        NX_LOGX(lm("Failed to issue password reset code for account %1")
            .arg(accountEmail), cl_logDEBUG1);
        return dbResult;
    }

    RestorePasswordNotification notification;
    notification.setAddressee(accountEmail);
    notification.setActivationCode(confirmationCode->code);

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, notification = std::move(notification)]()
        {
            m_emailManager->sendAsync(
                std::move(notification),
                std::function<void(bool)>());
        });

    return nx::db::DBResult::ok;
}

void AccountManager::temporaryCredentialsSaved(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    api::ResultCode resultCode,
    const std::string& accountEmail,
    api::TemporaryCredentials temporaryCredentials,
    std::function<void(api::ResultCode, api::TemporaryCredentials)> completionHandler)
{
    if (resultCode != api::ResultCode::ok)
    {
        NX_LOG(lm("%1 (%2). Failed to save temporary credentials (%3)")
            .arg(kAccountCreateTemporaryCredentialsPath).arg(accountEmail)
            .arg((int)resultCode), cl_logDEBUG1);
        return completionHandler(
            resultCode,
            api::TemporaryCredentials());
    }

    return completionHandler(
        api::ResultCode::ok,
        std::move(temporaryCredentials));
}

} // namespace cdb
} // namespace nx
