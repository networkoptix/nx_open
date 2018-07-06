#include "temporary_account_password_manager.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

#include <QtSql/QSqlQuery>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/crc32.h>
#include <nx/utils/cryptographic_random_device.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>
#include <nx/utils/string.h>
#include <nx/utils/time.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/uuid.h>

#include "../stree/cdb_ns.h"

namespace nx {
namespace cdb {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (TemporaryAccountCredentialsEx),
    (sql_record),
    _Fields)

TemporaryAccountPasswordManager::TemporaryAccountPasswordManager(
    const nx::utils::stree::ResourceNameSet& attrNameset,
    nx::sql::AsyncSqlQueryExecutor* const dbManager) noexcept(false)
:
    m_attrNameset(attrNameset),
    m_dbManager(dbManager)
{
    if (fillCache() != nx::sql::DBResult::ok)
        throw std::runtime_error("Failed to fill temporary account password cache");
}

TemporaryAccountPasswordManager::~TemporaryAccountPasswordManager()
{
    // Waiting for async calls to complete.
    m_startedAsyncCallsCounter.wait();
}

void TemporaryAccountPasswordManager::authenticateByName(
    const nx::network::http::StringType& username,
    const std::function<bool(const nx::Buffer&)>& checkPasswordHash,
    nx::utils::stree::ResourceContainer* const authProperties,
    nx::utils::MoveOnlyFunc<void(api::ResultCode)> completionHandler)
{
    QnMutexLocker lk(&m_mutex);

    auto authResultCode = api::ResultCode::notAuthorized;
    boost::optional<const TemporaryAccountCredentialsEx&> credentials =
        findMatchingCredentials(lk, username.toStdString(), checkPasswordHash, &authResultCode);

    if (!credentials)
        return completionHandler(authResultCode);

    authProperties->put(cdb::attr::credentialsId, QString::fromStdString(credentials->id));

    // Preparing output data.
    authProperties->put(
        cdb::attr::authAccountEmail,
        QString::fromStdString(credentials->accountEmail));
    if (credentials->isEmailCode)
        authProperties->put(cdb::attr::authenticatedByEmailCode, true);

    completionHandler(api::ResultCode::ok);
}

void TemporaryAccountPasswordManager::registerTemporaryCredentials(
    const AuthorizationInfo& /*authzInfo*/,
    data::TemporaryAccountCredentials tmpPasswordData,
    std::function<void(api::ResultCode)> completionHandler)
{
    TemporaryAccountCredentialsEx tmpPasswordDataInternal(std::move(tmpPasswordData));
    tmpPasswordDataInternal.id = QnUuid::createUuid().toSimpleString().toStdString();

    using namespace std::placeholders;
    m_dbManager->executeUpdate<TemporaryAccountCredentialsEx>(
        std::bind(&TemporaryAccountPasswordManager::insertTempPassword, this, _1, _2),
        std::move(tmpPasswordDataInternal),
        [this, locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::sql::QueryContext* /*queryContext*/,
                nx::sql::DBResult dbResult,
                TemporaryAccountCredentialsEx tempPasswordData)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                NX_LOG(lm("add_temporary_account_password (%1). "
                        "Failed to save password to DB. DB error: %2")
                    .arg(tempPasswordData.accountEmail).arg(dbResult),
                    cl_logDEBUG1);
            }

            return completionHandler(dbResultToApiResult(dbResult));
        });
}

void TemporaryAccountPasswordManager::addRandomCredentials(
    const std::string& accountEmail,
    data::TemporaryAccountCredentials* const data)
{
    if (data->login.empty())
    {
        data->login = generateRandomPassword();
        const auto loginCrc32 = std::to_string(nx::utils::crc32(accountEmail));
        data->login += "-" + loginCrc32;
    }

    data->password = generateRandomPassword();
    data->passwordHa1 = nx::network::http::calcHa1(
        data->login.c_str(),
        data->realm.c_str(),
        data->password.c_str()).constData();
}

nx::sql::DBResult TemporaryAccountPasswordManager::removeTemporaryPasswordsFromDbByAccountEmail(
    nx::sql::QueryContext* const queryContext,
    std::string accountEmail)
{
    QSqlQuery removeTempPasswordsQuery(*queryContext->connection());
    removeTempPasswordsQuery.prepare(
        R"sql(
        DELETE FROM account_password
        WHERE account_id=(SELECT id FROM account WHERE email=?)
        )sql");
    removeTempPasswordsQuery.addBindValue(QnSql::serialized_field(accountEmail));
    if (!removeTempPasswordsQuery.exec())
    {
        NX_LOGX(lm("Failed to remove temporary passwords of account %1. %2")
            .arg(accountEmail).arg(removeTempPasswordsQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

void TemporaryAccountPasswordManager::removeTemporaryPasswordsFromCacheByAccountEmail(
    std::string accountEmail)
{
    QnMutexLocker lk(&m_mutex);
    auto& temporaryCredentialsByEmail = m_temporaryCredentials.get<kIndexByAccountEmail>();
    temporaryCredentialsByEmail.erase(accountEmail);
}

nx::sql::DBResult TemporaryAccountPasswordManager::registerTemporaryCredentials(
    nx::sql::QueryContext* const queryContext,
    data::TemporaryAccountCredentials tempPasswordData)
{
    TemporaryAccountCredentialsEx tmpPasswordDataInternal(
        std::move(tempPasswordData));
    tmpPasswordDataInternal.id =
        QnUuid::createUuid().toSimpleString().toStdString();

    return insertTempPassword(
        queryContext,
        std::move(tmpPasswordDataInternal));
}

nx::sql::DBResult TemporaryAccountPasswordManager::fetchTemporaryCredentials(
    nx::sql::QueryContext* const queryContext,
    const data::TemporaryAccountCredentials& tempPasswordData,
    data::Credentials* credentials)
{
    QSqlQuery fetchTempPasswordQuery(*queryContext->connection());
    fetchTempPasswordQuery.prepare(R"sql(
        SELECT ap.login, ap.password_ha1 AS passwordString
        FROM account_password ap, account a
        WHERE ap.account_id=a.id AND a.email=:accountEmail
            AND max_use_count=:maxUseCount
            AND is_email_code=:isEmailCode
            AND access_rights=:accessRights
            AND prolongation_period_sec=:prolongationPeriodSec
            AND use_count=0
        )sql");
    QnSql::bind(tempPasswordData, &fetchTempPasswordQuery);
    fetchTempPasswordQuery.bindValue(
        ":accessRights",
        QnSql::serialized_field(tempPasswordData.accessRights.toString(m_attrNameset)));
    if (!fetchTempPasswordQuery.exec())
    {
        NX_LOG(lm("Error fetching temporary password for account %1. %2")
            .arg(tempPasswordData.accountEmail).arg(fetchTempPasswordQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::sql::DBResult::ioError;
    }

    if (!fetchTempPasswordQuery.next())
        return nx::sql::DBResult::notFound;

    credentials->login = fetchTempPasswordQuery.value("login").toString().toStdString();
    const auto passwordString =
        fetchTempPasswordQuery.value("passwordString").toString().toStdString();

    std::string passwordHa1;
    parsePasswordString(passwordString, &passwordHa1, &credentials->password);
    if (credentials->password.empty()) //< True for credentials created before this code has been written.
        return nx::sql::DBResult::notFound;

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult TemporaryAccountPasswordManager::updateCredentialsAttributes(
    nx::sql::QueryContext* const queryContext,
    const data::Credentials& credentials,
    const data::TemporaryAccountCredentials& tempPasswordData)
{
    QSqlQuery updateTempPasswordQuery(*queryContext->connection());
    updateTempPasswordQuery.prepare(
        R"sql(
        UPDATE account_password SET
            expiration_timestamp_utc = :expirationTimestampUtc,
            prolongation_period_sec = :prolongationPeriodSec,
            max_use_count = :maxUseCount,
            access_rights = :accessRights,
            is_email_code = :isEmailCode
        WHERE
            login = :login AND password_ha1 = :passwordString
        )sql");
    QnSql::bind(tempPasswordData, &updateTempPasswordQuery);
    updateTempPasswordQuery.bindValue(
        ":accessRights",
        QnSql::serialized_field(tempPasswordData.accessRights.toString(m_attrNameset)));

    const auto passwordHa1 = nx::network::http::calcHa1(
        credentials.login.c_str(),
        tempPasswordData.realm.c_str(),
        credentials.password.c_str()).toStdString();

    auto passwordString = preparePasswordString(
        passwordHa1,
        credentials.password);
    updateTempPasswordQuery.bindValue(":login", QnSql::serialized_field(credentials.login));
    updateTempPasswordQuery.bindValue(
        ":passwordString",
        QnSql::serialized_field(passwordString));
    if (!updateTempPasswordQuery.exec())
    {
        NX_LOG(lm("Could not update temporary password for account %1. %2")
            .arg(tempPasswordData.accountEmail).arg(updateTempPasswordQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::sql::DBResult::ioError;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        std::bind(&TemporaryAccountPasswordManager::updateCredentialsInCache, this,
            credentials, tempPasswordData));

    return nx::sql::DBResult::ok;
}

boost::optional<TemporaryAccountCredentialsEx>
    TemporaryAccountPasswordManager::getCredentialsByLogin(
        const std::string& login) const
{
    QnMutexLocker lk(&m_mutex);
    const auto& temporaryCredentialsByLogin = m_temporaryCredentials.get<kIndexByLogin>();
    auto it = temporaryCredentialsByLogin.find(login);
    if (it == temporaryCredentialsByLogin.cend())
        return boost::none;
    return *it;
}

bool TemporaryAccountPasswordManager::authorize(
    const std::string& credentialsId,
    const nx::utils::stree::AbstractResourceReader& inputData) const
{
    QnMutexLocker lock(&m_mutex);

    const auto& temporaryCredentialsById = m_temporaryCredentials.get<kIndexById>();
    auto it = temporaryCredentialsById.find(credentialsId);
    if (it == temporaryCredentialsById.end())
    {
        NX_DEBUG(this, lm("Authentication has been done by unknown credentials (id %1)")
            .args(credentialsId));
        return false;
    }

    if (!it->accessRights.authorize(inputData))
        return false;

    const_cast<TemporaryAccountPasswordManager*>(this)->
        runExpirationRulesOnSuccessfulLogin(lock, *it);

    return true;
}

std::string TemporaryAccountPasswordManager::generateRandomPassword() const
{
    auto str = QnUuid::createUuid().toSimpleByteArray().toLower();
    str.replace('-', "");
    return str.toStdString();
}

bool TemporaryAccountPasswordManager::isTemporaryPasswordExpired(
    const TemporaryAccountCredentialsEx& temporaryCredentials) const
{
    const bool isExpired =
        (temporaryCredentials.expirationTimestampUtc > 0) &&
        (temporaryCredentials.expirationTimestampUtc <= nx::utils::timeSinceEpoch().count());

    return isExpired
        || temporaryCredentials.useCount >= temporaryCredentials.maxUseCount;
}

void TemporaryAccountPasswordManager::removeTemporaryCredentialsFromDbDelayed(
    const TemporaryAccountCredentialsEx& temporaryCredentials)
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<std::string>(
        std::bind(&TemporaryAccountPasswordManager::deleteTempPassword, this, _1, _2),
        temporaryCredentials.id,
        [locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            nx::sql::QueryContext* /*queryContext*/,
            nx::sql::DBResult /*resultCode*/,
            std::string /*tempPasswordId*/)
        {
        });
}

nx::sql::DBResult TemporaryAccountPasswordManager::fillCache()
{
    using namespace std::placeholders;

    nx::utils::promise<nx::sql::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();
    m_dbManager->executeSelect(
        std::bind(&TemporaryAccountPasswordManager::fetchTemporaryPasswords, this, _1),
        [&cacheFilledPromise](
            nx::sql::QueryContext* /*queryContext*/,
            nx::sql::DBResult dbResult)
        {
            cacheFilledPromise.set_value(dbResult);
        });

    //waiting for completion
    future.wait();
    return future.get();
}

nx::sql::DBResult TemporaryAccountPasswordManager::fetchTemporaryPasswords(
    nx::sql::QueryContext* queryContext)
{
    QSqlQuery readPasswordsQuery(*queryContext->connection());
    readPasswordsQuery.setForwardOnly(true);
    readPasswordsQuery.prepare(
        "SELECT ap.id,                                                          \
            a.email as accountEmail,                                            \
            ap.login as login,                                                  \
            ap.password_ha1 as passwordHa1,                                     \
            ap.realm,                                                           \
            ap.expiration_timestamp_utc as expirationTimestampUtc,              \
            ap.prolongation_period_sec as prolongationPeriodSec,                \
            ap.max_use_count as maxUseCount,                                    \
            ap.use_count as useCount,                                           \
            ap.is_email_code as isEmailCode,                                    \
            ap.access_rights as accessRightsStr                                 \
         FROM account_password ap LEFT JOIN account a ON ap.account_id=a.id");
    if (!readPasswordsQuery.exec())
    {
        NX_LOG(lit("Failed to read temporary passwords from DB. %1").
            arg(readPasswordsQuery.lastError().text()), cl_logWARNING);
        return nx::sql::DBResult::ioError;
    }

    std::vector<TemporaryAccountCredentialsEx> tmpPasswords;
    QnSql::fetch_many(readPasswordsQuery, &tmpPasswords);
    for (auto& tmpPassword: tmpPasswords)
    {
        // TODO: #ak Currently, password_ha1 is actually password_ha1[:plain-text password].
        // Will switch to a single plain-text password field in a future version.
        std::string password;
        parsePasswordString(tmpPassword.passwordHa1, &tmpPassword.passwordHa1, &password);

        tmpPassword.accessRights.parse(
            m_attrNameset,
            tmpPassword.accessRightsStr);
        std::string login = tmpPassword.login;
        m_temporaryCredentials.insert(std::move(tmpPassword));
    }

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult TemporaryAccountPasswordManager::insertTempPassword(
    nx::sql::QueryContext* const queryContext,
    TemporaryAccountCredentialsEx tempPasswordData)
{
    QSqlQuery insertTempPasswordQuery(*queryContext->connection());
    insertTempPasswordQuery.prepare(
        R"sql(
        INSERT INTO account_password (id, account_id, login, password_ha1, realm,
            expiration_timestamp_utc, prolongation_period_sec, max_use_count,
            use_count, is_email_code, access_rights)
        VALUES (:id, (SELECT id FROM account WHERE email=:accountEmail), :login,
            :passwordString, :realm, :expirationTimestampUtc, :prolongationPeriodSec,
            :maxUseCount, :useCount, :isEmailCode, :accessRights)
        )sql");
    QnSql::bind(tempPasswordData, &insertTempPasswordQuery);
    insertTempPasswordQuery.bindValue(
        ":accessRights",
        QnSql::serialized_field(tempPasswordData.accessRights.toString(m_attrNameset)));

    auto passwordString = preparePasswordString(
        tempPasswordData.passwordHa1,
        tempPasswordData.password);
    insertTempPasswordQuery.bindValue(
        ":passwordString",
        QnSql::serialized_field(passwordString));
    if (!insertTempPasswordQuery.exec())
    {
        NX_LOG(lm("Could not insert temporary password for account %1 into DB. %2")
            .arg(tempPasswordData.accountEmail).arg(insertTempPasswordQuery.lastError().text()),
            cl_logDEBUG1);
        return nx::sql::DBResult::ioError;
    }

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, tempPasswordData = std::move(tempPasswordData)]() mutable
        {
            saveTempPasswordToCache(std::move(tempPasswordData));
        });

    return nx::sql::DBResult::ok;
}

void TemporaryAccountPasswordManager::saveTempPasswordToCache(
    TemporaryAccountCredentialsEx tempPasswordData)
{
    QnMutexLocker lk(&m_mutex);
    std::string login = tempPasswordData.login;
    m_temporaryCredentials.insert(std::move(tempPasswordData));
}

nx::sql::DBResult TemporaryAccountPasswordManager::deleteTempPassword(
    nx::sql::QueryContext* const queryContext,
    std::string tempPasswordID)
{
    QSqlQuery deleteTempPassword(*queryContext->connection());
    deleteTempPassword.prepare("DELETE FROM account_password WHERE id=:id");
    deleteTempPassword.bindValue(":id", QnSql::serialized_field(tempPasswordID));
    if (!deleteTempPassword.exec())
    {
        NX_LOG(lm("Could not delete temporary password %1 from DB. %2")
            .arg(tempPasswordID).arg(deleteTempPassword.lastError().text()),
            cl_logDEBUG1);
        return nx::sql::DBResult::ioError;
    }

    return nx::sql::DBResult::ok;
}

boost::optional<const TemporaryAccountCredentialsEx&>
    TemporaryAccountPasswordManager::findMatchingCredentials(
        const QnMutexLockerBase& /*lk*/,
        const std::string& username,
        std::function<bool(const nx::Buffer&)> checkPasswordHash,
        api::ResultCode* authResultCode)
{
    *authResultCode = api::ResultCode::badUsername;

    auto& temporaryCredentialsByLogin = m_temporaryCredentials.get<kIndexByLogin>();
    auto tmpPasswordsRange = temporaryCredentialsByLogin.equal_range(username);
    if (tmpPasswordsRange.first == temporaryCredentialsByLogin.end())
        return boost::none;

    for (auto it = tmpPasswordsRange.first; it != tmpPasswordsRange.second; )
    {
        auto curIt = it++;
        if (isTemporaryPasswordExpired(*curIt))
        {
            // TODO: #ak It is not intuitive that credentials are removed in this method.
            removeTemporaryCredentialsFromDbDelayed(*curIt);
            temporaryCredentialsByLogin.erase(curIt);
            continue;
        }

        *authResultCode = api::ResultCode::notAuthorized;

        if (!checkPasswordHash(curIt->passwordHa1.c_str()))
            continue;

        *authResultCode = api::ResultCode::ok;
        return *curIt;
    }

    return boost::none;
}

void TemporaryAccountPasswordManager::runExpirationRulesOnSuccessfulLogin(
    const QnMutexLockerBase& lk,
    const TemporaryAccountCredentialsEx& temporaryCredentials)
{
    auto& uniqueIndexById = m_temporaryCredentials.get<kIndexById>();
    auto it = uniqueIndexById.find(temporaryCredentials.id);
    if (it == uniqueIndexById.end())
    {
        NX_ASSERT(false);
        return;
    }

    updateExpirationRulesAfterSuccessfulLogin(lk, uniqueIndexById, it);
    if (isTemporaryPasswordExpired(*it))
    {
        removeTemporaryCredentialsFromDbDelayed(*it);
        uniqueIndexById.erase(it);
    }
}

template<typename Index, typename Iterator>
void TemporaryAccountPasswordManager::updateExpirationRulesAfterSuccessfulLogin(
    const QnMutexLockerBase& /*lk*/,
    Index& index,
    Iterator it)
{
    index.modify(
        it,
        [](TemporaryAccountCredentialsEx& value) { ++value.useCount; });

    // TODO: #ak prolonging expiration period if present.
}

void TemporaryAccountPasswordManager::parsePasswordString(
    const std::string& passwordString,
    std::string* passwordHa1,
    std::string* password)
{
    std::vector<std::string> passwordParts;
    boost::algorithm::split(
        passwordParts,
        passwordString,
        boost::algorithm::is_any_of(":"),
        boost::algorithm::token_compress_on);
    if (passwordParts.size() >= 1)
        *passwordHa1 = passwordParts[0];
    if (passwordParts.size() >= 2)
        *password = passwordParts[1];
}

std::string TemporaryAccountPasswordManager::preparePasswordString(
    const std::string& passwordHa1,
    const std::string& password)
{
    std::vector<std::string> passwordParts;
    passwordParts.reserve(2);
    passwordParts.push_back(passwordHa1);
    if (!password.empty())
        passwordParts.push_back(password);
    return boost::algorithm::join(passwordParts, ":");
}

void TemporaryAccountPasswordManager::updateCredentialsInCache(
    const data::Credentials& credentials,
    const data::TemporaryAccountCredentials& tempPasswordData)
{
    QnMutexLocker lock(&m_mutex);

    auto& temporaryCredentialsByEmail = m_temporaryCredentials.get<kIndexByLogin>();
    auto it = temporaryCredentialsByEmail.lower_bound(credentials.login);
    for (;
        (it != temporaryCredentialsByEmail.end()) && (it->login == credentials.login);
        ++it)
    {
        if (it->password != credentials.password)
            continue;

        temporaryCredentialsByEmail.modify(
            it,
            [&tempPasswordData](
                TemporaryAccountCredentialsEx& temporaryAccountCredentials)
            {
                temporaryAccountCredentials.expirationTimestampUtc =
                    tempPasswordData.expirationTimestampUtc;
                temporaryAccountCredentials.prolongationPeriodSec =
                    tempPasswordData.prolongationPeriodSec;
                temporaryAccountCredentials.maxUseCount = tempPasswordData.maxUseCount;
                temporaryAccountCredentials.accessRights = tempPasswordData.accessRights;
                temporaryAccountCredentials.isEmailCode = tempPasswordData.isEmailCode;
            });
    }
}

} // namespace cdb
} // namespace nx
