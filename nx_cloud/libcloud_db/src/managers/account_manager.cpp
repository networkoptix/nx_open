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

#include "email_manager.h"
#include "http_handlers/activate_account_handler.h"
#include "settings.h"
#include "stree/cdb_ns.h"
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
    std::function<void(api::ResultCode, data::AccountActivationCode)> completionHandler )
{
    accountData.id = QnUuid::createUuid();

    //fetching request source
    bool requestSourceSecured = false;
    authzInfo.get(attr::secureSource, &requestSourceSecured);

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountData, data::AccountActivationCode>(
        std::bind(&AccountManager::insertAccount, this, _1, _2, _3),
        std::move(accountData),
        std::bind(&AccountManager::accountAdded, this, requestSourceSecured, _1, _2, _3, std::move(completionHandler)) );
}

void AccountManager::activate(
    const AuthorizationInfo& /*authzInfo*/,
    data::AccountActivationCode emailVerificationCode,
    std::function<void(api::ResultCode)> completionHandler )
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::AccountActivationCode, QnUuid>(
        std::bind( &AccountManager::verifyAccount, this, _1, _2, _3 ),
        std::move( emailVerificationCode ),
        std::bind( &AccountManager::accountVerified, this, _1, _2, _3, std::move( completionHandler ) ) );
}

void AccountManager::getAccount(
    const AuthorizationInfo& authzInfo,
    std::function<void(api::ResultCode, data::AccountData)> completionHandler )
{
    QnUuid accountID;
    if( !authzInfo.get( attr::accountID, &accountID ) )
    {
        completionHandler(api::ResultCode::forbidden, data::AccountData());
        return;
    }

    if( auto accountData = m_cache.find( accountID ) )
    {
        accountData.get().passwordHa1.clear();
        completionHandler(api::ResultCode::ok, std::move(accountData.get()));
        return;
    }

    //very strange: account has been authenticated, but not found in the database
    completionHandler(api::ResultCode::notFound, data::AccountData());
}

boost::optional<data::AccountData> AccountManager::findAccountByID(const QnUuid& id) const
{
    return m_cache.find(id);
}

boost::optional<data::AccountData> AccountManager::findAccountByUserName(
    const nx::String& userName ) const
{
    //TODO #ak improve search
    return m_cache.findIf(
        [&]( const std::pair<const QnUuid, data::AccountData>& val ) {
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
               "full_name as fullName, status_code as statusCode "
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
        auto idCopy = account.id;
        if( !m_cache.insert( std::move(idCopy), std::move(account) ) )
        {
            assert( false );
        }
    }

    return db::DBResult::ok;
}

db::DBResult AccountManager::insertAccount(
    QSqlDatabase* const connection,
    const data::AccountData& accountData,
    data::AccountActivationCode* const resultData )
{
    //TODO #ak should return specific error if email address already used for account

    //inserting account
    QSqlQuery insertAccountQuery( *connection );
    insertAccountQuery.prepare( 
        "INSERT INTO account (id, email, password_ha1, full_name, status_code) "
                    "VALUES  (:id, :email, :passwordHa1, :fullName, :statusCode)");
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
    insertEmailVerificationQuery.bindValue( 0, QnSql::serialized_field(accountData.id) );
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
    confirmationUrl.setPath( ActivateAccountHandler::HANDLER_PATH );
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
    bool requestSourceSecured,
    db::DBResult resultCode,
    data::AccountData accountData,
    data::AccountActivationCode resultData,
    std::function<void(api::ResultCode, data::AccountActivationCode)> completionHandler )
{
    accountData.statusCode = api::AccountStatus::awaitingActivation;

    if( resultCode == db::DBResult::ok )
    {
        //updating cache
        m_cache.insert( accountData.id, std::move(accountData) );
    }

    //adding activation code only in response to portal
    completionHandler(
        resultCode == db::DBResult::ok
        ? api::ResultCode::ok
        : api::ResultCode::dbError,
        requestSourceSecured
            ? std::move(resultData)
            : data::AccountActivationCode());
}

nx::db::DBResult AccountManager::verifyAccount(
    QSqlDatabase* const connection,
    const data::AccountActivationCode& verificationCode,
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
    updateAccountStatus.bindValue( 0, static_cast<int>(api::AccountStatus::activated) );
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
    data::AccountActivationCode /*verificationCode*/,
    const QnUuid accountID,
    std::function<void(api::ResultCode)> completionHandler )
{
    if( resultCode == db::DBResult::ok )
    {
        m_cache.atomicUpdate(
            accountID, 
            []( api::AccountData& account ){ account.statusCode = api::AccountStatus::activated; } );
    }

    completionHandler( fromDbResultCode( resultCode ) );
}


}   //cdb
}   //nx
