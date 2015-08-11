/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "account_manager.h"

#include <functional>


namespace cdb {

void AccountManager::addAccount(
    const AuthorizationInfo& authzInfo,
    data::AccountData&& accountData,
    std::function<void(ResultCode, data::EmailVerificationCode)> completionHandler )
{
    //TODO #ak
    
    using namespace std::placeholders;
    db::DBManager::instance()->executeUpdate<data::AccountData, data::EmailVerificationCode>(
        std::bind(&AccountManager::insertAccount, this, _1, _2, _3),
        std::move(accountData),
        std::bind(&AccountManager::accountAdded, this, _1, _2, _3, std::move(completionHandler)) );
}

void AccountManager::verifyAccountEmailAddress(
    const AuthorizationInfo& authzInfo,
    const std::basic_string<uint8_t>& emailVerificationCode,
    std::function<void(ResultCode)> completionHandler )
{
    //TODO #ak
}

void AccountManager::getAccountByLogin(
    const AuthorizationInfo& authzInfo,
    const std::string& userName,
    std::function<void(ResultCode, data::AccountData)> completionHandler )
{
    //TODO #ak
}

db::DBResult AccountManager::insertAccount(
    QSqlDatabase* const tran,
    const data::AccountData& accountData,
    data::EmailVerificationCode* const resultData )
{
    //TODO #ak executing sql query
    
    
    return db::DBResult::ok;
}

void AccountManager::accountAdded(
    db::DBResult resultCode,
    data::AccountData accountData,
    data::EmailVerificationCode resultData,
    std::function<void(ResultCode, data::EmailVerificationCode)> completionHandler )
{
    //TODO #ak
    if( resultCode == db::DBResult::ok )
    {
        //TODO updating cache
        //m_cache.add( accountData.id, std::move(accountData) );
        //TODO sending email
        //EmailManager::instance()->send( accountData.email, ... );
    }
    
    completionHandler(
        resultCode == db::DBResult::ok ? ResultCode::ok : ResultCode::dbError,
        std::move( resultData ) );
}

}   //cdb
