/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "account_manager.h"


namespace cdb {


void AccountManager::addAccount(
    const AuthorizationInfo& authInfo,
    const data::AccountData& accountData,
    std::function<void(ResultCode)> completionHandler )
{
    //TODO #ak

    completionHandler( ResultCode::ok );
}

void AccountManager::verifyAccountEmailAddress(
    const AuthenticationInfo& authInfo,
    const std::basic_string<uint8_t>& emailVerificationCode,
    std::function<void(ResultCode)> completionHandler )
{
    //TODO #ak
}

void AccountManager::getAccountByLogin(
    const AuthenticationInfo& authInfo,
    const std::string& userName,
    std::function<void(ResultCode, data::AccountData)> completionHandler )
{
    //TODO #ak
}

}   //cdb
