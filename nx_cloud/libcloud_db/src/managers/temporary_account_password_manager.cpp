/**********************************************************
* Dec 15, 2015
* akolesnikov
***********************************************************/

#include "temporary_account_password_manager.h"

#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>
#include <utils/common/guard.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/fusion/serialization/sql_functions.h>

#include "stree/cdb_ns.h"


namespace nx {
namespace cdb {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (TemporaryAccountPasswordEx),
    (sql_record),
    _Fields)


TemporaryAccountPasswordManager::TemporaryAccountPasswordManager(
    const conf::Settings& settings,
    nx::db::AsyncSqlQueryExecutor* const dbManager) throw(std::runtime_error)
:
    m_settings(settings),
    m_dbManager(dbManager)
{
    if (fillCache() != db::DBResult::ok)
        throw std::runtime_error("Failed to fill temporary account password cache");
}

TemporaryAccountPasswordManager::~TemporaryAccountPasswordManager()
{
    //waiting for async calls to complete
    m_startedAsyncCallsCounter.wait();
}

void TemporaryAccountPasswordManager::authenticateByName(
    const nx_http::StringType& username,
    std::function<bool(const nx::Buffer&)> validateHa1Func,
    const stree::AbstractResourceReader& authSearchInputData,
    stree::ResourceContainer* const authProperties,
    nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    api::ResultCode result = api::ResultCode::notAuthorized;
    auto scopedGuard = makeScopedGuard(
        [/*std::move*/ &completionHandler, &result]()
        {
            completionHandler(result);
        });

    QnMutexLocker lk(&m_mutex);

    auto tmpPasswordsRange = m_accountPassword.equal_range(toStdString(username));
    if (tmpPasswordsRange.first == m_accountPassword.end())
        return;

    for (auto it = tmpPasswordsRange.first; it != tmpPasswordsRange.second; )
    {
        auto curIt = it++;
        if (!checkTemporaryPasswordForExpiration(&lk, curIt))
            continue;

        //currently, checking password permissions here, but should move it to authorization phase
        if (validateHa1Func(curIt->second.passwordHa1.c_str()) &&
            curIt->second.accessRights.authorize(authSearchInputData))
        {
            authProperties->put(
                cdb::attr::authAccountEmail,
                username);
            if (curIt->second.isEmailCode)
                authProperties->put(
                    cdb::attr::authenticatedByEmailCode,
                    true);

            result = api::ResultCode::ok;

            ++curIt->second.useCount;
            checkTemporaryPasswordForExpiration(&lk, curIt);
            return;
        }
    }
}

void TemporaryAccountPasswordManager::createTemporaryPassword(
    const AuthorizationInfo& /*authzInfo*/,
    data::TemporaryAccountPassword tmpPasswordData,
    std::function<void(api::ResultCode)> completionHandler)
{
    TemporaryAccountPasswordEx tmpPasswordDataInternal(std::move(tmpPasswordData));
    tmpPasswordDataInternal.id = QnUuid::createUuid().toSimpleString().toStdString();

    using namespace std::placeholders;
    m_dbManager->executeUpdate<TemporaryAccountPasswordEx>(
        std::bind(&TemporaryAccountPasswordManager::insertTempPassword, this, _1, _2),
        std::move(tmpPasswordDataInternal),
        std::bind(&TemporaryAccountPasswordManager::tempPasswordAddedToDb, this,
            m_startedAsyncCallsCounter.getScopedIncrement(),
            _1, _2, std::move(completionHandler)));
}

bool TemporaryAccountPasswordManager::checkTemporaryPasswordForExpiration(
    QnMutexLockerBase* const /*lk*/,
    std::multimap<std::string, TemporaryAccountPasswordEx>::iterator passwordIter)
{
    if (passwordIter->second.useCount >= passwordIter->second.maxUseCount ||
        (passwordIter->second.expirationTimestampUtc > 0 &&
            passwordIter->second.expirationTimestampUtc <= ::time(NULL)))
    {
        std::string tmpPasswordID = passwordIter->second.id;
        m_accountPassword.erase(passwordIter);

        //removing password from DB
        using namespace std::placeholders;
        m_dbManager->executeUpdate<std::string>(
            std::bind(&TemporaryAccountPasswordManager::deleteTempPassword, this, _1, _2),
            std::move(tmpPasswordID),
            std::bind(&TemporaryAccountPasswordManager::tempPasswordDeleted, this,
                m_startedAsyncCallsCounter.getScopedIncrement(),
                _1, _2, std::function<void(api::ResultCode)>()));
        return false;
    }

    return true;
}

db::DBResult TemporaryAccountPasswordManager::fillCache()
{
    nx::utils::promise<db::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();
    using namespace std::placeholders;
    m_dbManager->executeSelect<int>(
        std::bind(&TemporaryAccountPasswordManager::fetchTemporaryPasswords, this, _1, _2),
        [&cacheFilledPromise](db::DBResult dbResult, int /*dummyResult*/) {
            cacheFilledPromise.set_value(dbResult);
        });

    //waiting for completion
    future.wait();
    return future.get();
}

nx::db::DBResult TemporaryAccountPasswordManager::fetchTemporaryPasswords(
    QSqlDatabase* connection,
    int* const /*dummyResult*/)
{
    QSqlQuery readPasswordsQuery(*connection);
    readPasswordsQuery.prepare(
        "SELECT ap.id,                                                          \
            a.email as accountEmail,                                            \
            ap.password_ha1 as passwordHa1,                                     \
            ap.realm,                                                           \
            ap.expiration_timestamp_utc as expirationTimestampUtc,              \
            ap.max_use_count as maxUseCount,                                    \
            ap.use_count as useCount,                                           \
            ap.is_email_code as isEmailCode,                                    \
            ap.access_rights as accessRightsStr                                 \
         FROM account_password ap LEFT JOIN account a ON ap.account_id=a.id");
    if (!readPasswordsQuery.exec())
    {
        NX_LOG(lit("Failed to read temporary passwords from DB. %1").
            arg(connection->lastError().text()), cl_logWARNING);
        return db::DBResult::ioError;
    }

    std::vector<TemporaryAccountPasswordEx> tmpPasswords;
    QnSql::fetch_many(readPasswordsQuery, &tmpPasswords);
    for (auto& tmpPassword : tmpPasswords)
    {
        tmpPassword.accessRights.parse(tmpPassword.accessRightsStr);
        std::string email = tmpPassword.accountEmail;
        m_accountPassword.emplace(
            std::move(email),
            std::move(tmpPassword));
    }

    return db::DBResult::ok;
}

nx::db::DBResult TemporaryAccountPasswordManager::insertTempPassword(
    QSqlDatabase* const connection,
    TemporaryAccountPasswordEx tempPasswordData)
{
    QSqlQuery insertTempPassword(*connection);
    insertTempPassword.prepare(
        "INSERT INTO account_password (id, account_id, password_ha1, realm, "
            "expiration_timestamp_utc, max_use_count, use_count, is_email_code, access_rights) "
        "VALUES (:id, (SELECT id FROM account WHERE email=:accountEmail), :passwordHa1, :realm, "
            ":expirationTimestampUtc, :maxUseCount, :useCount, :isEmailCode, :accessRights)");
    QnSql::bind(tempPasswordData, &insertTempPassword);
    insertTempPassword.bindValue(
        ":accessRights",
        QnSql::serialized_field(tempPasswordData.accessRights.toString()));
    if (!insertTempPassword.exec())
    {
        NX_LOG(lm("Could not insert temporary password for account %1 into DB. %2").
            arg(tempPasswordData.accountEmail).arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

void TemporaryAccountPasswordManager::tempPasswordAddedToDb(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::DBResult resultCode,
    TemporaryAccountPasswordEx tempPasswordData,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (resultCode != db::DBResult::ok)
    {
        NX_LOG(lm("add_temporary_account_password (%1). Failed to save password to DB. DB error: %2").
            arg(tempPasswordData.accountEmail).arg((int)resultCode), cl_logDEBUG1);
        return completionHandler(fromDbResultCode(resultCode));
    }

    //placing temporary password to internal cache
    {
        QnMutexLocker lk(&m_mutex);
        std::string email = tempPasswordData.accountEmail;
        m_accountPassword.emplace(
            std::move(email),
            std::move(tempPasswordData));
    }

    return completionHandler(api::ResultCode::ok);
}

nx::db::DBResult TemporaryAccountPasswordManager::deleteTempPassword(
    QSqlDatabase* const connection,
    std::string tempPasswordID)
{
    QSqlQuery deleteTempPassword(*connection);
    deleteTempPassword.prepare("DELETE FROM account_password WHERE id=:id");
    deleteTempPassword.bindValue(":id", QnSql::serialized_field(tempPasswordID));
    if (!deleteTempPassword.exec())
    {
        NX_LOG(lm("Could not delete temporary password %1 from DB. %2").
            arg(tempPasswordID).arg(connection->lastError().text()), cl_logDEBUG1);
        return db::DBResult::ioError;
    }

    return db::DBResult::ok;
}

void TemporaryAccountPasswordManager::tempPasswordDeleted(
    QnCounter::ScopedIncrement /*asyncCallLocker*/,
    nx::db::DBResult resultCode,
    std::string /*tempPasswordID*/,
    std::function<void(api::ResultCode)> completionHandler)
{
    if (completionHandler)
        completionHandler(fromDbResultCode(resultCode));
}

}   //cdb
}   //nx
