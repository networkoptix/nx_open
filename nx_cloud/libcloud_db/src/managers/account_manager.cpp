/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "account_manager.h"

#include <functional>
#include <iostream>

#include <QtCore/QDateTime>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <utils/serialization/sql.h>
#include <utils/serialization/sql_functions.h>
#include <nx/email/mustache/mustache_helper.h>

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
        std::bind(&AccountManager::accountAdded, this, requestSourceSecured, _1, _2, _3, std::move(completionHandler)) );
}

void AccountManager::activate(
    const AuthorizationInfo& /*authzInfo*/,
    data::AccountConfirmationCode emailVerificationCode,
    std::function<void(api::ResultCode)> completionHandler )
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountConfirmationCode, std::string>(
        std::bind( &AccountManager::verifyAccount, this, _1, _2, _3 ),
        std::move( emailVerificationCode ),
        std::bind( &AccountManager::accountVerified, this, _1, _2, _3, std::move( completionHandler ) ) );
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
        std::bind(&AccountManager::accountUpdated, this, _1, _2, std::move(completionHandler)));
}

void AccountManager::resetPassword(
    const AuthorizationInfo& authzInfo,
    data::AccountEmail accountEmail,
    std::function<void(api::ResultCode, data::AccountConfirmationCode)> completionHandler)
{

}

boost::optional<data::AccountData> AccountManager::findAccountByUserName(
    const std::string& userName ) const
{
    //TODO #ak improve search
    return m_cache.findIf(
        [&]( const std::pair<const std::string, data::AccountData>& val ) {
            return val.second.email == userName;
        } );
}

db::DBResult AccountManager::fillCache()
{
    std::promise<db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();

    //starting async operation
    using namespace std::placeholders;
    m_dbManager->executeSelect<int>(
        std::bind( &AccountManager::fetchAccounts, this, _1, _2 ),
        [&cacheFilledPromise]( db::DBResult dbResult, int /*dummy*/ ) {
            cacheFilledPromise.set_value( dbResult );
        } );

    //waiting for completion
    future.wait();
    return future.get();
}

db::DBResult AccountManager::fetchAccounts( QSqlDatabase* connection, int* const /*dummy*/ )
{
    QSqlQuery readAccountsQuery( *connection );
    readAccountsQuery.prepare(
        "SELECT id, email, password_ha1 as passwordHa1, "
               "full_name as fullName, customization, status_code as statusCode "
        "FROM account" );
    if( !readAccountsQuery.exec() )
    {
        NX_LOG( lit( "Failed to read account list from DB. %1" ).
            arg( connection->lastError().text() ), cl_logWARNING );
        return db::DBResult::ioError;
    }

    std::vector<data::AccountData> accounts;
    QnSql::fetch_many( readAccountsQuery, &accounts );

    for( auto& account: accounts )
    {
        auto emailCopy = account.email;
        if( !m_cache.insert( std::move(emailCopy), std::move(account) ) )
        {
            assert( false );
        }
    }

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
    nx::db::DBResult resultCode,
    data::AccountConfirmationCode /*verificationCode*/,
    const std::string accountEmail,
    std::function<void(api::ResultCode)> completionHandler )
{
    if( resultCode == db::DBResult::ok )
    {
        m_cache.atomicUpdate(
            accountEmail,
            []( api::AccountData& account ){ account.statusCode = api::AccountStatus::activated; } );
    }

    completionHandler( fromDbResultCode( resultCode ) );
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

}   //cdb
}   //nx
