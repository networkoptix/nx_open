/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_account_manager_h
#define cloud_db_account_manager_h

#include <functional>
#include <string>
#include <vector>

#include <utils/common/singleton.h>

#include "access_control/types.h"
#include "data/account_data.h"
#include "types.h"


namespace cdb {

class AccountManager
:
    public Singleton<AccountManager>
{
public:
    //!Adds account in "not activated" state and sends verification email to the provided address
    void addAccount(
        const AuthorizationInfo& authzInfo,
        const data::AccountData& accountData,
        std::function<void(ResultCode, data::EmailVerificationCode)> completionHandler );
    //!On success, account moved to "activated" state
    void verifyAccountEmailAddress(
        const AuthorizationInfo& authzInfo,
        const std::basic_string<uint8_t>& emailVerificationCode,
        std::function<void(ResultCode)> completionHandler );
    
    //void getAccounts(/*TODO*/);
    void getAccountByLogin(
        const AuthorizationInfo& authzInfo,
        const std::string& userName,
        std::function<void(ResultCode, data::AccountData)> completionHandler );
};

}   //cdb

#endif
