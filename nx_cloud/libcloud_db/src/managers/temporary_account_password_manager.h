/**********************************************************
* Dec 15, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_TEMPORARY_ACCOUNT_PASSWORD_MANAGER_H
#define NX_CDB_TEMPORARY_ACCOUNT_PASSWORD_MANAGER_H

#include "access_control/abstract_authentication_data_provider.h"

#include <nx/utils/thread/mutex.h>
#include <utils/common/counter.h>
#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/db/async_sql_query_executor.h>

#include "access_control/auth_types.h"
#include "data/account_data.h"
#include "managers_types.h"


namespace nx {
namespace cdb {

namespace conf
{
class Settings;
}

class TemporaryAccountPasswordEx
:
    public data::TemporaryAccountPassword
{
public:
    std::string id;
    std::string accessRightsStr;

    TemporaryAccountPasswordEx() {}
    TemporaryAccountPasswordEx(const TemporaryAccountPasswordEx& right)
    :
        data::TemporaryAccountPassword(right),
        id(right.id),
        accessRightsStr(right.accessRightsStr)
    {
    }
    TemporaryAccountPasswordEx(data::TemporaryAccountPassword&& right)
    :
        data::TemporaryAccountPassword(std::move(right))
    {
    }
};

#define TemporaryAccountPasswordEx_Fields TemporaryAccountPassword_Fields (id)(accessRightsStr)

class TemporaryAccountPasswordManager
:
    public AbstractAuthenticationDataProvider
{
public:
    TemporaryAccountPasswordManager(
        const conf::Settings& settings,
        nx::db::AsyncSqlQueryExecutor* const dbManager) throw(std::runtime_error);
    virtual ~TemporaryAccountPasswordManager();

    virtual void authenticateByName(
        const nx_http::StringType& username,
        std::function<bool(const nx::Buffer&)> validateHa1Func,
        const stree::AbstractResourceReader& authSearchInputData,
        stree::ResourceContainer* const authProperties,
        nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler) override;

    void createTemporaryPassword(
        const AuthorizationInfo& authzInfo,
        data::TemporaryAccountPassword tmpPasswordData,
        std::function<void(api::ResultCode)> completionHandler);

private:
    const conf::Settings& m_settings;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    QnCounter m_startedAsyncCallsCounter;
    //!map<account email, password data>
    std::multimap<std::string, TemporaryAccountPasswordEx> m_accountPassword;
    mutable QnMutex m_mutex;

    bool checkTemporaryPasswordForExpiration(
        QnMutexLockerBase* const lk,
        std::multimap<std::string, TemporaryAccountPasswordEx>::iterator passwordIter);

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchTemporaryPasswords(
        QSqlDatabase* connection,
        int* const /*dummyResult*/);

    nx::db::DBResult insertTempPassword(
        QSqlDatabase* const connection,
        TemporaryAccountPasswordEx tempPasswordData);
    void tempPasswordAddedToDb(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult resultCode,
        TemporaryAccountPasswordEx tempPasswordData,
        std::function<void(api::ResultCode)> completionHandler);

    nx::db::DBResult deleteTempPassword(
        QSqlDatabase* const connection,
        std::string tempPasswordID);
    void tempPasswordDeleted(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::DBResult resultCode,
        std::string tempPasswordID,
        std::function<void(api::ResultCode)> completionHandler);
   
};

}   //cdb
}   //nx

#endif  //NX_CDB_TEMPORARY_ACCOUNT_PASSWORD_MANAGER_H
