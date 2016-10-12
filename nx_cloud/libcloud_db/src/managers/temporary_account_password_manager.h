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

class TemporaryAccountCredentialsEx
:
    public data::TemporaryAccountCredentials
{
public:
    std::string id;
    std::string accessRightsStr;

    TemporaryAccountCredentialsEx() {}
    TemporaryAccountCredentialsEx(const TemporaryAccountCredentialsEx& right)
    :
        data::TemporaryAccountCredentials(right),
        id(right.id),
        accessRightsStr(right.accessRightsStr)
    {
    }
    TemporaryAccountCredentialsEx(data::TemporaryAccountCredentials&& right)
    :
        data::TemporaryAccountCredentials(std::move(right))
    {
    }
};

#define TemporaryAccountCredentialsEx_Fields TemporaryAccountCredentials_Fields (id)(accessRightsStr)

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

    void registerTemporaryCredentials(
        const AuthorizationInfo& authzInfo,
        data::TemporaryAccountCredentials tmpPasswordData,
        std::function<void(api::ResultCode)> completionHandler);

    std::string generateRandomPassword() const;
    /**
     * Adds password and password digest.
     * If \a data->login is empty, random login is generated
     */
    void addRandomCredentials(data::TemporaryAccountCredentials* const data);

    nx::db::DBResult removeTemporaryPasswordsFromDbByAccountEmail(
        nx::db::QueryContext* const queryContext,
        std::string accountEmail);
    void removeTemporaryPasswordsFromCacheByAccountEmail(
        std::string accountEmail);

    nx::db::DBResult registerTemporaryCredentials(
        nx::db::QueryContext* const queryContext,
        data::TemporaryAccountCredentials tempPasswordData);

    boost::optional<TemporaryAccountCredentialsEx> getCredentialsByLogin(
        const std::string& login) const;

private:
    const conf::Settings& m_settings;
    nx::db::AsyncSqlQueryExecutor* const m_dbManager;
    QnCounter m_startedAsyncCallsCounter;
    //!map<login, password data>
    std::multimap<std::string, TemporaryAccountCredentialsEx> m_temporaryCredentialsByLogin;
    mutable QnMutex m_mutex;

    bool checkTemporaryPasswordForExpiration(
        QnMutexLockerBase* const lk,
        std::multimap<std::string, TemporaryAccountCredentialsEx>::iterator passwordIter);

    nx::db::DBResult fillCache();
    nx::db::DBResult fetchTemporaryPasswords(
        nx::db::QueryContext* queryContext,
        int* const /*dummyResult*/);

    nx::db::DBResult insertTempPassword(
        nx::db::QueryContext* const queryContext,
        TemporaryAccountCredentialsEx tempPasswordData);
    void saveTempPasswordToCache(TemporaryAccountCredentialsEx tempPasswordData);

    nx::db::DBResult deleteTempPassword(
        nx::db::QueryContext* const queryContext,
        std::string tempPasswordID);
    void tempPasswordDeleted(
        QnCounter::ScopedIncrement asyncCallLocker,
        nx::db::QueryContext* /*queryContext*/,
        nx::db::DBResult resultCode,
        std::string tempPasswordID,
        std::function<void(api::ResultCode)> completionHandler);
   
};

}   //cdb
}   //nx

#endif  //NX_CDB_TEMPORARY_ACCOUNT_PASSWORD_MANAGER_H
