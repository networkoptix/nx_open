/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "account_manager.h"


namespace cdb {


void AccountManager::addAccount(
    const AuthorizationInfo& authzInfo,
    const data::AccountData& accountData,
    std::function<void(ResultCode, data::EmailVerificationCode)> completionHandler )
{
    //TODO #ak

    completionHandler( ResultCode::ok, data::EmailVerificationCode() );
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

}   //cdb
