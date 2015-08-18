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
#include "settings.h"
#include "version.h"


namespace nx {
namespace cdb {

static const QString CONFIRMAION_EMAIL_TEMPLATE_FILE_NAME =
    lit( ":/email_templates/account_confirmation.mustache" );
static const QString CLOUD_BACKEND_URL_PARAM = lit( "cloudUrl" );
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
    m_dbManager->executeUpdate<data::EmailVerificationCode>(
        std::bind( &AccountManager::verifyAccount, this, _1, _2 ),
        std::move( emailVerificationCode ),
        std::bind( &AccountManager::accountVerified, this, _1, _2, std::move( completionHandler ) ) );
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
        return db::DBResult::ioError;

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
        return db::DBResult::ioError;
    resultData->code.assign( emailVerificationCode.constData(), emailVerificationCode.size() );

    //sending confirmation email
    QVariantHash emailParams;
    emailParams[CLOUD_BACKEND_URL_PARAM] = m_settings.cloudBackendUrl(); //TODO #ak use something like QN_CLOUD_BACKEND_URL;
    emailParams[CONFIRMATION_CODE_PARAM] = QString::fromStdString( resultData->code );
    m_emailManager->renderAndSendEmailAsync(
        QString::fromStdString( accountData.email ),
        CONFIRMAION_EMAIL_TEMPLATE_FILE_NAME,
        emailParams );

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
    QSqlDatabase* const tran,
    const data::EmailVerificationCode& verificationCode )
{
    //TODO #ak
    return db::DBResult::ioError;
}

void AccountManager::accountVerified(
    nx::db::DBResult resultCode,
    data::EmailVerificationCode verificationCode,
    std::function<void( ResultCode )> completionHandler )
{
    if( resultCode == db::DBResult::ok )
    {
        //TODO #ak updating cache
        //m_cache.add( accountData.id, std::move( accountData ) );
    }

    completionHandler( resultCode == db::DBResult::ok ? ResultCode::ok : ResultCode::dbError );
    //TODO #ak
}


}   //cdb
}   //nx
