/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "account_manager.h"

#include <functional>
#include <iostream>

#include <QtCore/QDateTime>
#include <QtSql/QSqlQuery>

#include <cloud_db_client/src/cdb_request_path.h>
#include <cloud_db_client/src/data/types.h>
#include <nx/email/mustache/mustache_helper.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/log/log.h>
#include <utils/common/guard.h>
#include <utils/serialization/sql.h>
#include <utils/serialization/sql_functions.h>

#include "access_control/authentication_manager.h"
#include "email_manager.h"
#include "http_handlers/activate_account_handler.h"
#include "notification.h"
#include "settings.h"
#include "stree/cdb_ns.h"
#include "version.h"


namespace nx {
namespace cdb {

static const QString CONFIRMAION_EMAIL_TEMPLATE_FILE_NAME = lit("activate_account");
static const QString CONFIRMATION_URL_PARAM = lit("confirmationUrl");
static const QString CONFIRMATION_CODE_PARAM = lit("confirmationCode");
static const int UNCONFIRMED_ACCOUNT_EXPIRATION_SEC = 3*24*60*60;

AccountManager::AccountManager(
    const conf::Settings& settings,
    nx::db::DBManager* const dbManager,
    EMailManager* const emailManager ) throw( std::runtime_error )
:
    m_settings( settings ),
    m_dbManager( dbManager ),
    m_emailManager( emailManager )
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
    stree::AbstractResourceWriter* const authProperties,
    std::function<void(bool)> completionHandler)
{
    bool result = false;
    auto scopedGuard = makeScopedGuard(
        [/*std::move*/ completionHandler, &result](){
            completionHandler(result);
        });

    QnMutexLocker lk(&m_mutex);

    auto account = m_cache.find(toStdString(username));
    if (!account)
        return;
    if (validateHa1Func(account->passwordHa1.c_str()))
    {
        authProperties->put(
            cdb::attr::authAccountEmail,
            username);
        result = true;
        return;
    }

    auto tmpPasswordsRange = m_accountPassword.equal_range(toStdString(username));
    if (tmpPasswordsRange.first == m_accountPassword.end())
        return;

    for (auto it = tmpPasswordsRange.first; it != tmpPasswordsRange.second; )
    {
        auto curIt = it++;
        if (!checkTemporaryPasswordForExpiration(&lk, curIt))
            continue;

        if (validateHa1Func(curIt->second.passwordHa1.c_str()))
        {
            authProperties->put(
                cdb::attr::authAccountEmail,
                username);
            result = true;

            ++curIt->second.useCount;
            checkTemporaryPasswordForExpiration(&lk, curIt);
            return;
        }
    }
}

void AccountManager::addAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountData accountData,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler )
{
    if (const auto existingAccount = m_cache.find(accountData.email))
    {
        NX_LOG(lm("Failed to add account with already used email %1").
            arg(accountData.email), cl_logDEBUG1);
        return completionHandler(
            api::ResultCode::alreadyExists,
            data::AccountConfirmationCode());
    }

    accountData.id = QnUuid::createUuid();

    //fetching request source
    bool requestSourceSecured = false;
    authzInfo.get(attr::secureSource, &requestSourceSecured);

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountData, data::AccountConfirmationCode>(
        std::bind(&AccountManager::insertAccount, this, _1, _2, _3),
        std::move(accountData),
        std::bind(&AccountManager::accountAdded, this, 
                    m_startedAsyncCallsCounter.getScopedIncrement(),
                    requestSourceSecured,
                    _1, _2, _3, std::move(completionHandler)) );
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
                    _1, _2, _3, std::move(completionHandler)));
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

    //very strange: account has been authenticated, but not found in the database
    completionHandler(api::ResultCode::notFound, data::AccountData());
}

void AccountManager::updateAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountUpdateData accountData,
    std::function<void(api::ResultCode)> completionHandler)
{
    std::string accountEmail;
    if (!authzInfo.get( attr::authAccountEmail, &accountEmail))
    {
        completionHandler(api::ResultCode::forbidden);
        return;
    }

    data::AccountUpdateDataWithEmail updateDataWithEmail(std::move(accountData));
    updateDataWithEmail.email = accountEmail;

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountUpdateDataWithEmail>(
        std::bind(&AccountManager::updateAccountInDB, this, _1, _2),
        std::move(updateDataWithEmail),
        std::bind(&AccountManager::accountUpdated, this,
                    m_startedAsyncCallsCounter.getScopedIncrement(),
                    _1, _2, std::move(completionHandler)));
}

void AccountManager::resetPassword(
    const AuthorizationInfo& authzInfo,
    data::AccountEmail accountEmail,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{
    //fetching request source
    bool requestSourceSecured = false;
    authzInfo.get(attr::secureSource, &requestSourceSecured);

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountEmail, data::TemporaryAccountPassword>(
        std::bind(&AccountManager::generatePasswordResetCode, this, _1, _2, _3),
        std::move(accountEmail),
        std::bind(&AccountManager::passwordResetCodeGenerated, this,
                    m_startedAsyncCallsCounter.getScopedIncrement(),
                    requestSourceSecured, _1, _2, _3, std::move(completionHandler)));
}

boost::optional<data::AccountData> AccountManager::findAccountByUserName(
    const std::string& userName ) const
{
    return m_cache.find(userName);
}

db::DBResult AccountManager::fillCache()
{
    std::promise<db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    using namespace std::placeholders;
    m_dbManager->executeSelect<int>(
        std::bind( &AccountManager::fetchAccounts, this, _1, _2 ),
        [&cacheFilledPromise]( db::DBResult dbResult, int /*dummyResult*/ ) {
            cacheFilledPromise.set_value( dbResult );
        } );

    //waiting for completion
    future.wait();
    const auto result = future.get();
    if (result != db::DBResult::ok)
        return result;

    //reading temporary passwords
    cacheFilledPromise = std::promise<db::DBResult>();
    future = cacheFilledPromise.get_future();
    using namespace std::placeholders;
    m_dbManager->executeSelect<int>(
        std::bind(&AccountManager::fetchTemporaryPasswords, this, _1, _2),
        [&cacheFilledPromise](db::DBResult dbResult, int /*dummyResult*/) {
            cacheFilledPromise.set_value(dbResult);
        });

    //waiting for completion
    future.wait();
    return future.get();
}

db::DBResult AccountManager::fetchAccounts( QSqlDatabase* connection, int* const /*dummyResult*/ )
{
    QSqlQuery readAccountsQuery(*connection);
    readAccountsQuery.prepare(
        "SELECT id, email, password_ha1 as passwordHa1, "
               "full_name as fullName, customization, status_code as statusCode "
        "FROM account" );
    if (!readAccountsQuery.exec())
    {
        NX_LOG( lit( "Failed to read account list from DB. %1" ).
            arg( connection->lastError().text() ), cl_logWARNING );
        return db::DBResult::ioError;
    }

    std::vector<data::AccountData> accounts;
    QnSql::fetch_many( readAccountsQuery, &accounts );

    for (auto& account : accounts)
    {
        auto emailCopy = account.email;
        if (!m_cache.insert(std::move(emailCopy), std::move(account)))
        {
            assert( false );
        }
    }

    return db::DBResult::ok;
}

class TemporaryAccountPasswordWithEmail
:
    public data::TemporaryAccountPassword
{
public:
    std::string email;
};

#define TemporaryAccountPasswordWithEmail_Fields TemporaryAccountPassword_Fields (email)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (TemporaryAccountPasswordWithEmail),
    (sql_record),
    _Fields)


db::DBResult AccountManager::fetchTemporaryPasswords(QSqlDatabase* connection, int* const /*dummyResult*/)
{
    QSqlQuery readPasswordsQuery(*connection);
    readPasswordsQuery.prepare(
        "SELECT ap.id,                                                          \
            ap.account_id as accountID,                                         \
            a.email,                                                            \
            ap.password_ha1 as passwordHa1,                                     \
            ap.realm,                                                           \
            ap.expiration_timestamp_utc as expirationTimestampUtc,              \
            ap.max_use_count as maxUseCount,                                    \
            ap.use_count as useCount,                                           \
            ap.access_rights as accessRights                                    \
         FROM account_password ap LEFT JOIN account a ON ap.account_id=a.id");
    if (!readPasswordsQuery.exec())
    {
        NX_LOG(lit("Failed to read temporary passwords from DB. %1").
            arg(connection->lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    std::vector<TemporaryAccountPasswordWithEmail> tmpPasswords;
    QnSql::fetch_many(readPasswordsQuery, &tmpPasswords);
    for (auto& tmpPassword: tmpPasswords)
        m_accountPassword.emplace(std::move(tmpPassword.email), std::move(tmpPassword));

    return db::DBResult::ok;
}

db::DBResult AccountManager::insertAccount(
    QSqlDatabase* const connection,
    const data::AccountData& accountData,
    data::AccountConfirmationCode* const resultData )
{
    //TODO #ak should return specific error if email address already used for account

    //inserting account
    QSqlQuery insertAccountQuery( *connection );
    insertAccountQuery.prepare( 
        "INSERT INTO account (id, email, password_ha1, full_name, customization, status_code) "
                    "VALUES  (:id, :email, :passwordHa1, :fullName, :customization, :statusCode)");
    QnSql::bind( accountData, &insertAccountQuery );
    insertAccountQuery.bindValue(
        ":statusCode",
        static_cast<int>(api::AccountStatus::awaitingActivation) );
    if( !insertAccountQuery.exec() )
    {
        NX_LOG( lit( "Could not insert account into DB. %1" ).
            arg( connection->lastError().text() ), cl_logDEBUG1 );
        return db::DBResult::ioError;
    }

    //inserting email verification code
    const auto emailVerificationCode = QnUuid::createUuid().toByteArray().toHex();
    QSqlQuery insertEmailVerificationQuery( *connection );
    insertEmailVerificationQuery.prepare( 
        "INSERT INTO email_verification( account_id, verification_code, expiration_date ) "
                               "VALUES ( ?, ?, ? )" );
    insertEmailVerificationQuery.bindValue(
        0,
        QnSql::serialized_field(accountData.id));
    insertEmailVerificationQuery.bindValue(
        1,
        emailVerificationCode);
    insertEmailVerificationQuery.bindValue(
        2,
        QDateTime::currentDateTimeUtc().addSecs(UNCONFIRMED_ACCOUNT_EXPIRATION_SEC));
    if( !insertEmailVerificationQuery.exec() )
    {
        NX_LOG( lit( "Could not insert account verification code into DB. %1" ).
            arg( connection->lastError().text() ), cl_logDEBUG1 );
        return db::DBResult::ioError;
    }
    resultData->code.assign(
        emailVerificationCode.constData(),
        emailVerificationCode.size());

    //sending confirmation email
    ActivateAccountNotification notification;
    notification.user_email = QString::fromStdString(accountData.email);
    notification.type = CONFIRMAION_EMAIL_TEMPLATE_FILE_NAME;
    notification.message.code = QString::fromStdString(resultData->code);
    m_emailManager->sendAsync(
        std::move(notification),
        std::function<void(bool)>());

    return db::DBResult::ok;
}

void AccountManager::accountAdded(
    ThreadSafeCounter::ScopedIncrement asyncCallLocker,
    bool requestSourceSecured,
    db::DBResult resultCode,
    data::AccountData accountData,
    data::AccountConfirmationCode resultData,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler )
{
    accountData.statusCode = api::AccountStatus::awaitingActivation;

    if( resultCode == db::DBResult::ok )
    {
        //updating cache
        auto email = accountData.email;
        m_cache.insert( std::move(email), std::move(accountData) );
    }

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
    QSqlDatabase* const connection,
    const data::AccountConfirmationCode& verificationCode,
    std::string* const resultAccountEmail )
{
    QSqlQuery getAccountByVerificationCode( *connection );
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

    QSqlQuery removeVerificationCode( *connection );
    removeVerificationCode.prepare( 
        "DELETE FROM email_verification WHERE verification_code LIKE :code" );
    QnSql::bind( verificationCode, &removeVerificationCode );
    if( !removeVerificationCode.exec() )
    {
        NX_LOG( lit( "Failed to remove account verification code %1 from DB. %2" ).
            arg( QString::fromStdString(verificationCode.code) ).arg( connection->lastError().text() ),
            cl_logDEBUG1 );
        return db::DBResult::ioError;
    }

    QSqlQuery updateAccountStatus( *connection );
    updateAccountStatus.prepare( 
        "UPDATE account SET status_code = ? WHERE email = ?" );
    updateAccountStatus.bindValue( 0, static_cast<int>(api::AccountStatus::activated) );
    updateAccountStatus.bindValue( 1, QnSql::serialized_field(accountEmail) );
    if( !updateAccountStatus.exec() )
    {
        NX_LOG(lm("Failed to update account %1 status. %2").
            arg(accountEmail).arg(connection->lastError().text()),
            cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    *resultAccountEmail = std::move(accountEmail);

    return db::DBResult::ok;
}

void AccountManager::accountVerified(
    ThreadSafeCounter::ScopedIncrement asyncCallLocker,
    nx::db::DBResult resultCode,
    data::AccountConfirmationCode /*verificationCode*/,
    const std::string accountEmail,
    std::function<void(api::ResultCode, api::AccountEmail)> completionHandler )
{
    if( resultCode != db::DBResult::ok )
        return completionHandler(
            fromDbResultCode(resultCode),
            api::AccountEmail());

    m_cache.atomicUpdate(
        accountEmail,
        []( api::AccountData& account ){ account.statusCode = api::AccountStatus::activated; } );
    api::AccountEmail response;
    response.email = accountEmail;
    completionHandler(
        fromDbResultCode(resultCode),
        std::move(response));
}

nx::db::DBResult AccountManager::updateAccountInDB(
    QSqlDatabase* const connection,
    const data::AccountUpdateDataWithEmail& accountData)
{
    assert(static_cast<bool>(accountData.passwordHa1) ||
           static_cast<bool>(accountData.fullName) ||
           static_cast<bool>(accountData.customization));
    
    QSqlQuery updateAccountQuery( *connection );
    QStringList accountUpdateFieldsSql;
    if (accountData.passwordHa1)
        accountUpdateFieldsSql << lit("password_ha1=:passwordHa1");
    if (accountData.fullName)
        accountUpdateFieldsSql << lit("full_name=:fullName");
    if (accountData.customization)
        accountUpdateFieldsSql << lit("customization=:customization");
    updateAccountQuery.prepare( 
        lit("UPDATE account SET %1 WHERE email=:email").
            arg(accountUpdateFieldsSql.join(lit(","))));
    //TODO #ak use fusion here (at the moment it is missing boost::optional support)
    if (accountData.passwordHa1)
        updateAccountQuery.bindValue(
            ":passwordHa1",
            QnSql::serialized_field(accountData.passwordHa1.get()));
    if (accountData.fullName)
        updateAccountQuery.bindValue(
            ":fullName",
            QnSql::serialized_field(accountData.fullName.get()));
    if (accountData.customization)
        updateAccountQuery.bindValue(
            ":customization",
            QnSql::serialized_field(accountData.customization.get()));
    if( !updateAccountQuery.exec() )
    {
        NX_LOG(lit("Could not update account in DB. %1").
            arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }
    
    return db::DBResult::ok;
}

void AccountManager::accountUpdated(
    ThreadSafeCounter::ScopedIncrement asyncCallLocker,
    nx::db::DBResult resultCode,
    data::AccountUpdateDataWithEmail accountData,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (resultCode == db::DBResult::ok)
    {
        m_cache.atomicUpdate(
            accountData.email,
            [&accountData](api::AccountData& account) {
                if (accountData.passwordHa1)
                    account.passwordHa1 = accountData.passwordHa1.get();
                if (accountData.fullName)
                    account.fullName = accountData.fullName.get();
                if (accountData.customization)
                    account.customization = accountData.customization.get();
            });
    }
    
    completionHandler(fromDbResultCode(resultCode));
}

nx::db::DBResult AccountManager::generatePasswordResetCode(
    QSqlDatabase* const connection,
    const data::AccountEmail& accountEmail,
    data::TemporaryAccountPassword* const tempPasswordData)
{
    auto account = m_cache.find(accountEmail.email);
    if (!account)
    {
        NX_LOG(lm("%1 (%2). Account not found").
            arg(kAccountPasswordResetPath).arg(accountEmail.email), cl_logDEBUG1);
        return db::DBResult::notFound;
    }

    tempPasswordData->accountID = account->id;
    tempPasswordData->realm = AuthenticationManager::realm().constData();
    std::string tempPassword(10 + (rand() % 10), 'c');
    std::generate(
        tempPassword.begin(),
        tempPassword.end(),
        [](){return 'a' + (rand() % ('z'-'a'));});
    tempPasswordData->password = tempPassword;
    tempPasswordData->passwordHa1 = nx_http::calcHa1(
        accountEmail.email.c_str(),
        tempPasswordData->realm.c_str(),
        tempPassword.c_str()).constData();
    tempPasswordData->expirationTimestampUtc =
        ::time(NULL) +
        m_settings.accountManager().passwordResetCodeExpirationTimeout.count();
    tempPasswordData->maxUseCount = 1;
    //tempPassword.accessRights = TODO #ak updateAccount only;

    nx::db::DBResult result = insertTempPassword(
        connection,
        *tempPasswordData);
    if (result != db::DBResult::ok)
        return result;

    return db::DBResult::ok;
}

void AccountManager::passwordResetCodeGenerated(
    ThreadSafeCounter::ScopedIncrement asyncCallLocker,
    bool requestSourceSecured,
    nx::db::DBResult resultCode,
    data::AccountEmail accountEmail,
    data::TemporaryAccountPassword resultData,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{
    if (resultCode != db::DBResult::ok)
    {
        NX_LOG(lm("/account/reset_password (%1). Failed to save password reset code. DB error: %2").
            arg(accountEmail.email).arg((int)resultCode), cl_logDEBUG1);
        return completionHandler(
            fromDbResultCode(resultCode),
            data::AccountConfirmationCode());
    }

    data::AccountConfirmationCode confirmationCode;
    auto resetCodeStr = resultData.password + ":" + accountEmail.email;
    confirmationCode.code = QByteArray::fromRawData(
        resetCodeStr.data(), resetCodeStr.size()).toBase64().constData();

    //placing temporary password to internal cache
    {
        QnMutexLocker lk(&m_mutex);
        resultData.password.clear();
        m_accountPassword.emplace(accountEmail.email, std::move(resultData));
    }

    return completionHandler(
        api::ResultCode::ok,
        requestSourceSecured
            ? std::move(confirmationCode)
            : data::AccountConfirmationCode());
}

nx::db::DBResult AccountManager::insertTempPassword(
    QSqlDatabase* const connection,
    data::TemporaryAccountPassword tempPasswordData)
{
    QSqlQuery insertTempPassword(*connection);
    insertTempPassword.prepare(
        "INSERT INTO account_password (id, account_id, password_ha1, realm, "
            "expiration_timestamp_utc, max_use_count, use_count, access_rights) "
        "VALUES (:id, :accountID, :passwordHa1, :realm, "
                ":expirationTimestampUtc, :maxUseCount, :useCount, :accessRights)");
    QnSql::bind(tempPasswordData, &insertTempPassword);
    insertTempPassword.bindValue(
        ":id",
        QnSql::serialized_field(QnUuid::createUuid()));
    if (!insertTempPassword.exec())
    {
        NX_LOG(lm("Could not insert temporary password for account %1 into DB. %2").
            arg(tempPasswordData.accountID).arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

bool AccountManager::checkTemporaryPasswordForExpiration(
    QnMutexLockerBase* const /*lk*/,
    std::multimap<std::string, data::TemporaryAccountPassword>::iterator passwordIter)
{
    if (passwordIter->second.useCount >= passwordIter->second.maxUseCount ||
        (passwordIter->second.expirationTimestampUtc > 0 &&
            passwordIter->second.expirationTimestampUtc <= ::time(NULL)))
    {
        //TODO #ak remove password from DB
        m_accountPassword.erase(passwordIter);
        return false;
    }

    return true;
}

}   //cdb
}   //nx
