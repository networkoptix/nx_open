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

/*!
    \note Methods of this class are re-enterable
*/
class AccountManager
:
    public Singleton<AccountManager>
{
public:
    //!Adds account in "not activated" state and sends verification email to the address provided
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
    
private:
    db::DBResult insertAccount(
        db::DBTransaction& tran,
        const data::AccountData& accountData,
        data::EmailVerificationCode* const resultData );
    void accountAdded(
        db::DBResult resultCode,
        data::AccountData&& accountData,
        data::EmailVerificationCode&& resultData,
        std::function<void(ResultCode, data::EmailVerificationCode)> completionHandler );

}   //cdb

#endif
