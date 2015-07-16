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


class AccountManager
:
    public Singleton<AccountManager>
{
public:
    //!Adds account in "not activated" state and sends verification email to the provided address
    void addAccount(
        const AuthorizationInfo& authzInfo,
        const AccountData& accountData,
        std::function<void(ResultCode)> completionHandler );
    //!On success, account moved to "activated" state
    void verifyAccountEmailAddress(
        const AuthenticationInfo& authInfo,
        const std::basic_string<uint8_t>& emailVerificationCode,
        std::function<void(ResultCode)> completionHandler );
    
    //void getAccounts(/*TODO*/);
    void getAccountByLogin(
        const AuthenticationInfo& authInfo,
        const std::string& userName,
        std::function<void(ResultCode, AccountData)> completionHandler );
};

#endif
