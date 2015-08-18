/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "account_manager.h"

#include <functional>

#include <QtCore/QDateTime>
#include <QtSql/QSqlQuery>

#include <mustache/mustache_helper.h>
#include <utils/common/log.h>
#include <utils/serialization/sql.h>
#include <utils/serialization/sql_functions.h>

#include "data/cdb_ns.h"
#include "email_manager.h"
#include "http_handlers/verify_email_address_handler.h"
#include "settings.h"
#include "version.h"


namespace nx {
namespace cdb {

static const QString CONFIRMAION_EMAIL_TEMPLATE_FILE_NAME =
    lit( ":/email_templates/account_confirmation.mustache" );
static const QString CONFIRMATION_URL_PARAM = lit( "confirmationUrl" );
static const QString CONFIRMATION_CODE_PARAM = lit( "confirmationCode" );
static const int UNCONFIRMED_ACCOUNT_EXPIRATION_SEC = 3*24*60*60;


AccountManager::AccountManager(
    const conf::Settings& settings,
    nx::db::DBManager* const dbManager,
    EMailManager* const emailManager )
:
    m_settings( settings ),
    m_dbManager( dbManager ),
    m_emailManager( emailManager )
{
}

void AccountManager::addAccount(
    const AuthorizationInfo& /*authzInfo*/,
    data::AccountData accountData,
    std::function<void(ResultCode)> completionHandler )
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountData, data::EmailVerificationCode>(
        std::bind(&AccountManager::insertAccount, this, _1, _2, _3),
        std::move(accountData),
        std::bind(&AccountManager::accountAdded, this, _1, _2, _3, std::move(completionHandler)) );
}

void AccountManager::verifyAccountEmailAddress(
    const AuthorizationInfo& /*authzInfo*/,
    data::EmailVerificationCode emailVerificationCode,
    std::function<void(ResultCode)> completionHandler )
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::EmailVerificationCode, QnUuid>(
        std::bind( &AccountManager::verifyAccount, this, _1, _2, _3 ),
        std::move( emailVerificationCode ),
        std::bind( &AccountManager::accountVerified, this, _1, _2, _3, std::move( completionHandler ) ) );
}

void AccountManager::getAccount(
    const AuthorizationInfo& authzInfo,
    std::function<void(ResultCode, data::AccountData)> completionHandler )
{
    QnUuid accountID;
    if( !authzInfo.get( param::accountID, &accountID ) )
    {
        completionHandler( ResultCode::notAuthorized, data::AccountData() );
        return;
    }

    data::AccountData resultData;
    if( !m_cache.get( accountID, &resultData ) )
    {
        //very strange: account has been authenticated, but not found in the database
        completionHandler( ResultCode::notFound, data::AccountData() );
        return;
    }

    completionHandler( ResultCode::ok, std::move(resultData) );
}

db::DBResult AccountManager::insertAccount(
    QSqlDatabase* const connection,
    const data::AccountData& accountData,
    data::EmailVerificationCode* const resultData )
{
    //TODO #ak should return specific error if email address already used for account

    //inserting account
    const QnUuid accountID = QnUuid::createUuid();
    QSqlQuery insertAccountQuery( *connection );
    insertAccountQuery.prepare( 
        "INSERT INTO account (id, email, password_ha1, full_name, status_code) "
                    "VALUES  (:id, :email, :passwordHa1, :fullName, :statusCode)");
    QnSql::bind( accountData, &insertAccountQuery );
    insertAccountQuery.bindValue( ":id", QnSql::serialized_field(accountID) );
    insertAccountQuery.bindValue( ":statusCode", data::asAwaitingEmailConfirmation );
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
    insertEmailVerificationQuery.bindValue( 0, QnSql::serialized_field( accountID ) );
    insertEmailVerificationQuery.bindValue( 1, emailVerificationCode );
    insertEmailVerificationQuery.bindValue( 2, QDateTime::currentDateTimeUtc().addSecs(UNCONFIRMED_ACCOUNT_EXPIRATION_SEC) );
    if( !insertEmailVerificationQuery.exec() )
    {
        NX_LOG( lit( "Could not insert account verification code into DB. %1" ).
            arg( connection->lastError().text() ), cl_logDEBUG1 );
        return db::DBResult::ioError;
    }
    resultData->code.assign( emailVerificationCode.constData(), emailVerificationCode.size() );

    //sending confirmation email
    QVariantHash emailParams;
    QUrl confirmationUrl( m_settings.cloudBackendUrl() );   //TODO #ak use something like QN_CLOUD_BACKEND_URL;
    confirmationUrl.setPath( VerifyEmailAddressHandler::HANDLER_PATH );
    confirmationUrl.setQuery( lit("code=%1").arg(QLatin1String(emailVerificationCode)) );  //TODO #ak replace with fusion::QnUrlQuery::serialized(*resultData)
    emailParams[CONFIRMATION_URL_PARAM] = confirmationUrl.toString();
    emailParams[CONFIRMATION_CODE_PARAM] = QString::fromStdString( resultData->code );
    if( !m_emailManager->renderAndSendEmailAsync(
            QString::fromStdString( accountData.email ),
            CONFIRMAION_EMAIL_TEMPLATE_FILE_NAME,
            emailParams,
            std::function<void(bool)>() ) )
    {
        NX_LOG( lit( "Failed to issue async account confirmation email send" ), cl_logDEBUG1 );
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

void AccountManager::accountAdded(
    db::DBResult resultCode,
    data::AccountData accountData,
    data::EmailVerificationCode resultData,
    std::function<void(ResultCode)> completionHandler )
{
    if( resultCode == db::DBResult::ok )
    {
        //updating cache
        m_cache.add( accountData.id, std::move(accountData) );
    }
    
    completionHandler( resultCode == db::DBResult::ok ? ResultCode::ok : ResultCode::dbError );
}

nx::db::DBResult AccountManager::verifyAccount(
    QSqlDatabase* const connection,
    const data::EmailVerificationCode& verificationCode,
    QnUuid* const resultAccountID )
{
    QSqlQuery getAccountByVerificationCode( *connection );
    getAccountByVerificationCode.prepare(
        "SELECT account_id FROM email_verification WHERE verification_code LIKE :code" );
    QnSql::bind( verificationCode, &getAccountByVerificationCode );
    if( !getAccountByVerificationCode.exec() )
        return db::DBResult::ioError;
    if( !getAccountByVerificationCode.next() )
    {
        NX_LOG( lit("Email verification code %1 was not found in the database").
            arg( QString::fromStdString(verificationCode.code) ), cl_logDEBUG1 );
        return db::DBResult::notFound;
    }
    QnUuid accountID = QnSql::deserialized_field<QnUuid>( getAccountByVerificationCode.value( 0 ) );

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
        "UPDATE account SET status_code = ? WHERE id = ?" );
    updateAccountStatus.bindValue( 0, data::asActivated );
    updateAccountStatus.bindValue( 1, QnSql::serialized_field(accountID) );
    if( !updateAccountStatus.exec() )
    {
        NX_LOG( lit( "Failed to update account %1 status. %2" ).
            arg( accountID.toString() ).arg( connection->lastError().text() ),
            cl_logDEBUG1 );
        return db::DBResult::ioError;
    }

    *resultAccountID = std::move( accountID );

    return db::DBResult::ok;
}

void AccountManager::accountVerified(
    nx::db::DBResult resultCode,
    data::EmailVerificationCode verificationCode,
    const QnUuid accountID,
    std::function<void( ResultCode )> completionHandler )
{
    if( resultCode == db::DBResult::ok )
    {
        m_cache.atomicUpdate(
            accountID, 
            []( data::AccountData& account ){ account.statusCode = data::AccountStatus::asActivated; } );
    }

    completionHandler( fromDbResultCode( resultCode ) );
}


}   //cdb
}   //nx
