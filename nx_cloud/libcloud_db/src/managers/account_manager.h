/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_account_manager_h
#define cloud_db_account_manager_h

#include <functional>
#include <string>
#include <vector>

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/db/db_manager.h>
#include <utils/network/buffer.h>

#include "access_control/auth_types.h"
#include "cache.h"
#include "data/account_data.h"
#include "managers_types.h"


namespace nx {
namespace cdb {

class EMailManager;
namespace conf
{
    class Settings;
}

/*!
    \note Methods of this class are re-enterable
*/
class AccountManager
{
public:
    /*!
        \throw std::runtime_error In case of failure to pre-fill data cache
    */
    AccountManager(
        const conf::Settings& settings,
        nx::db::DBManager* const dbManager,
        EMailManager* const emailManager ) throw( std::runtime_error );

    //!Adds account in "not activated" state and sends verification email to the email address provided
    void addAccount(
        const AuthorizationInfo& authzInfo,
        data::AccountData accountData,
        std::function<void(api::ResultCode, data::AccountActivationCode)> completionHandler );
    //!On success, account moved to "activated" state
    void activate(
        const AuthorizationInfo& authzInfo,
        data::AccountActivationCode emailVerificationCode,
        std::function<void(api::ResultCode)> completionHandler );
    
    //!Retrieves account corresponding to authorization data \a authzInfo
    void getAccount(
        const AuthorizationInfo& authzInfo,
        std::function<void(api::ResultCode, data::AccountData)> completionHandler );

    boost::optional<data::AccountData> findAccountByID(const QnUuid& id) const;
    boost::optional<data::AccountData> findAccountByUserName(
        const nx::String& userName) const;
    
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
        data::AccountActivationCode* const resultData );
    void accountAdded(
        bool requestSourceSecured,
        nx::db::DBResult resultCode,
        data::AccountData accountData,
        data::AccountActivationCode resultData,
        std::function<void(api::ResultCode, data::AccountActivationCode)> completionHandler );

    //verify_account DB operations
    nx::db::DBResult verifyAccount(
        QSqlDatabase* const tran,
        const data::AccountActivationCode& verificationCode,
        QnUuid* const accountID );
    void accountVerified(
        nx::db::DBResult resultCode,
        data::AccountActivationCode verificationCode,
        const QnUuid accountID,
        std::function<void(api::ResultCode)> completionHandler );
};

}   //cdb
}   //nx

#endif
