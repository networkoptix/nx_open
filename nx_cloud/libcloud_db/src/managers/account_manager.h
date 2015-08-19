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

#include "access_control/auth_types.h"
#include "data/account_data.h"
#include "data/email_verification_code.h"
#include "managers_types.h"


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
    /*!
        \return false if already exists
    */
    bool add( KeyType key, CachedType value )
    {
        QnMutexLocker lk( &m_mutex );
        return m_data.emplace( std::move(key), std::move(value) ).second;
    }

    bool get( const KeyType& key, CachedType* const value ) const
    {
        QnMutexLocker lk( &m_mutex );

        auto it = m_data.find( key );
        if( it == m_data.end() )
            return false;
        *value = it->second;
        return true;
    }

    //!Executes \a updateFunc on item with \a key
    /*!
        \warning \a updateFunc is executed with internal mutex locked, so it MUST NOT BLOCK!
        \return \a true if item found and updated. \a false otherwise
    */
    template<class Func>
    bool atomicUpdate(
        const KeyType& key,
        const Func& updateFunc )
    {
        QnMutexLocker lk( &m_mutex );

        auto it = m_data.find( key );
        if( it == m_data.end() )
            return false;
        updateFunc( it->second );
        return true;
    }
    std::map<KeyType, CachedType> m_data;

private:
    mutable QnMutex m_mutex;
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

    //!MUST be called before any other methods
    /*!
        \return If \a false object cannot be used futher
    */
    bool init();

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

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchAccounts( QSqlDatabase* connection, int* const dummy );

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
        const data::EmailVerificationCode& verificationCode,
        QnUuid* const accountID );
    void accountVerified(
        nx::db::DBResult resultCode,
        data::EmailVerificationCode verificationCode,
        const QnUuid accountID,
        std::function<void( ResultCode )> completionHandler );
};

}   //cdb
}   //nx

#endif
