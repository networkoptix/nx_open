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
#include "db/db_manager.h"
#include "types.h"


namespace nx {
namespace cdb {

class EMailManager;
namespace conf
{
    class Settings;
}

//!Just a dummy for cache
template<class KeyType, class CachedType>
class Cache
{
public:
    void add( KeyType key, CachedType value )
    {
        //TODO #ak
    }
};

/*!
    \note Methods of this class are re-enterable
*/
class AccountManager
:
    public Singleton<AccountManager>
{
public:
    AccountManager(
        EMailManager* const emailManager,
        const conf::Settings& settings );

    //!Adds account in "not activated" state and sends verification email to the email address provided
    void addAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountData&& accountData,
        std::function<void(ResultCode)> completionHandler );
    //!On success, account moved to "activated" state
    void verifyAccountEmailAddress(
        const AuthorizationInfo& authzInfo,
        data::EmailVerificationCode&& emailVerificationCode,
        std::function<void(ResultCode)> completionHandler );
    
    void getAccountByLogin(
        const AuthorizationInfo& authzInfo,
        const std::string& userName,
        std::function<void(ResultCode, data::AccountData)> completionHandler );
    
private:
    EMailManager* const m_emailManager;
    const conf::Settings& m_settings;
    Cache<QnUuid, data::AccountData> m_cache;

    db::DBResult insertAccount(
        QSqlDatabase* const tran,
        const data::AccountData& accountData,
        data::EmailVerificationCode* const resultData );
    void accountAdded(
        db::DBResult resultCode,
        data::AccountData accountData,
        data::EmailVerificationCode resultData,
        std::function<void(ResultCode)> completionHandler );
};

}   //cdb
}   //nx

#endif
