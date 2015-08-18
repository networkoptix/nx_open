/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_account_manager_h
#define cloud_db_account_manager_h

#include <functional>
#include <string>
#include <vector>

#include <utils/db/db_manager.h>

#include "access_control/types.h"
#include "data/account_data.h"
#include "data/email_verification_code.h"
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
    void add( KeyType /*key*/, CachedType /*value*/ )
    {
        //TODO #ak
    }

    bool get( KeyType /*key*/, CachedType* const /*value*/ )
    {
        //TODO #ak
        return false;
    }
};

/*!
    \note Methods of this class are re-enterable
*/
class AccountManager
{
public:
    AccountManager(
        const conf::Settings& settings,
        nx::db::DBManager* const dbManager,
        EMailManager* const emailManager );

    //!Adds account in "not activated" state and sends verification email to the email address provided
    void addAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountData accountData,
        std::function<void(ResultCode)> completionHandler );
    //!On success, account moved to "activated" state
    void verifyAccountEmailAddress(
        const AuthorizationInfo& authzInfo,
        data::EmailVerificationCode emailVerificationCode,
        std::function<void(ResultCode)> completionHandler );
    
    //!Retrieves account corresponding to authorization data \a authzInfo
    void getAccount(
        const AuthorizationInfo& authzInfo,
        std::function<void(ResultCode, data::AccountData)> completionHandler );
    
private:
    const conf::Settings& m_settings;
    nx::db::DBManager* const m_dbManager;
    EMailManager* const m_emailManager;
    Cache<QnUuid, data::AccountData> m_cache;

    //add_account DB operations
    nx::db::DBResult insertAccount(
        QSqlDatabase* const tran,
        const data::AccountData& accountData,
        data::EmailVerificationCode* const resultData );
    void accountAdded(
        nx::db::DBResult resultCode,
        data::AccountData accountData,
        data::EmailVerificationCode resultData,
        std::function<void(ResultCode)> completionHandler );

    //verify_account DB operations
    nx::db::DBResult verifyAccount(
        QSqlDatabase* const tran,
        const data::EmailVerificationCode& verificationCode );
    void accountVerified(
        nx::db::DBResult resultCode,
        data::EmailVerificationCode resultData,
        std::function<void( ResultCode )> completionHandler );
};

}   //cdb
}   //nx

#endif
