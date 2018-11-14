#include "temporary_account_password_manager.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/sql_functions.h>
#include <nx/fusion/serialization/sql.h>
#include <nx/network/http/auth_tools.h>
#include <nx/utils/crc32.h>
#include <nx/utils/random_cryptographic_device.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/future.h>
#include <nx/utils/string.h>
#include <nx/utils/time.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/uuid.h>

#include "../stree/cdb_ns.h"

namespace nx::cdb {

TemporaryAccountPasswordManager::TemporaryAccountPasswordManager(
    const nx::utils::stree::ResourceNameSet& attributeNameset,
    nx::sql::AsyncSqlQueryExecutor* const dbManager) noexcept(false)
:
    m_attributeNameset(attributeNameset),
    m_dbManager(dbManager),
    m_dao(dao::TemporaryCredentialsDaoFactory::instance().create())
{
    deleteExpiredCredentials();

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
    boost::optional<const data::TemporaryAccountCredentialsEx&> credentials =
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
    data::TemporaryAccountCredentialsEx tmpPasswordDataInternal(std::move(tmpPasswordData));
    tmpPasswordDataInternal.id = QnUuid::createUuid().toSimpleString().toStdString();

    using namespace std::placeholders;
    m_dbManager->executeUpdate<data::TemporaryAccountCredentialsEx>(
        std::bind(&TemporaryAccountPasswordManager::insertTempPassword, this, _1, _2),
        std::move(tmpPasswordDataInternal),
        [this, locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)](
                nx::sql::DBResult dbResult,
                data::TemporaryAccountCredentialsEx tempPasswordData)
        {
            if (dbResult != nx::sql::DBResult::ok)
            {
                NX_DEBUG(this, lm("add_temporary_account_password (%1). "
                        "Failed to save password to DB. DB error: %2")
                    .arg(tempPasswordData.accountEmail).arg(dbResult));
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

void TemporaryAccountPasswordManager::removeTemporaryPasswordsFromDbByAccountEmail(
    nx::sql::QueryContext* const queryContext,
    std::string accountEmail)
{
    m_dao->removeByAccountEmail(queryContext, accountEmail);
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
    data::TemporaryAccountCredentialsEx tmpPasswordDataInternal(
        std::move(tempPasswordData));
    tmpPasswordDataInternal.id =
        QnUuid::createUuid().toSimpleString().toStdString();

    return insertTempPassword(
        queryContext,
        std::move(tmpPasswordDataInternal));
}

std::optional<data::Credentials> TemporaryAccountPasswordManager::fetchTemporaryCredentials(
    nx::sql::QueryContext* const queryContext,
    const data::TemporaryAccountCredentials& tempPasswordData)
{
    return m_dao->find(queryContext, m_attributeNameset, tempPasswordData);
}

void TemporaryAccountPasswordManager::updateCredentialsAttributes(
    nx::sql::QueryContext* const queryContext,
    const data::Credentials& credentials,
    const data::TemporaryAccountCredentials& tempPasswordData)
{
    m_dao->update(
        queryContext,
        m_attributeNameset,
        credentials,
        tempPasswordData);

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        std::bind(&TemporaryAccountPasswordManager::updateCredentialsInCache, this,
            credentials, tempPasswordData));
}

boost::optional<data::TemporaryAccountCredentialsEx>
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
    const data::TemporaryAccountCredentialsEx& temporaryCredentials) const
{
    const bool isExpired =
        (temporaryCredentials.expirationTimestampUtc > 0) &&
        (temporaryCredentials.expirationTimestampUtc <= nx::utils::timeSinceEpoch().count());

    return isExpired
        || temporaryCredentials.useCount >= temporaryCredentials.maxUseCount;
}

void TemporaryAccountPasswordManager::removeTemporaryCredentialsFromDbDelayed(
    const data::TemporaryAccountCredentialsEx& temporaryCredentials)
{
    using namespace std::placeholders;
    m_dbManager->executeUpdate<std::string>(
        std::bind(&TemporaryAccountPasswordManager::deleteTempPassword, this, _1, _2),
        temporaryCredentials.id,
        [locker = m_startedAsyncCallsCounter.getScopedIncrement()](
            nx::sql::DBResult /*resultCode*/,
            std::string /*tempPasswordId*/)
        {
        });
}

void TemporaryAccountPasswordManager::deleteExpiredCredentials()
{
    using namespace std::chrono;

    NX_VERBOSE(this, "Removing expired credentials");

    m_dbManager->executeUpdateQuerySync(
        [this](auto&&... args) { m_dao->deleteExpiredCredentials(std::move(args)...); });

    NX_DEBUG(this, "Removed expired credentials");
}

nx::sql::DBResult TemporaryAccountPasswordManager::fillCache()
{
    using namespace std::placeholders;

    nx::utils::promise<nx::sql::DBResult> cacheFilledPromise;
    auto future = cacheFilledPromise.get_future();
    m_dbManager->executeSelect(
        std::bind(&TemporaryAccountPasswordManager::fetchTemporaryPasswords, this, _1),
        [&cacheFilledPromise](nx::sql::DBResult dbResult)
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
    NX_DEBUG(this, "Filling temporary credentials cache");

    auto tmpPasswords = m_dao->fetchAll(queryContext, m_attributeNameset);

    NX_VERBOSE(this, lm("Fetched all temporary credentials"));

    for (auto& tmpPassword: tmpPasswords)
        m_temporaryCredentials.insert(std::move(tmpPassword));

    NX_DEBUG(this, lm("Restored %1 temporary credentials")
        .args(m_temporaryCredentials.size()));

    return nx::sql::DBResult::ok;
}

nx::sql::DBResult TemporaryAccountPasswordManager::insertTempPassword(
    nx::sql::QueryContext* const queryContext,
    data::TemporaryAccountCredentialsEx tempPasswordData)
{
    m_dao->insert(
        queryContext,
        m_attributeNameset,
        std::move(tempPasswordData));

    queryContext->transaction()->addOnSuccessfulCommitHandler(
        [this, tempPasswordData = std::move(tempPasswordData)]() mutable
        {
            saveTempPasswordToCache(std::move(tempPasswordData));
        });

    return nx::sql::DBResult::ok;
}

void TemporaryAccountPasswordManager::saveTempPasswordToCache(
    data::TemporaryAccountCredentialsEx tempPasswordData)
{
    QnMutexLocker lk(&m_mutex);
    std::string login = tempPasswordData.login;
    m_temporaryCredentials.insert(std::move(tempPasswordData));
}

nx::sql::DBResult TemporaryAccountPasswordManager::deleteTempPassword(
    nx::sql::QueryContext* const queryContext,
    std::string tempPasswordId)
{
    m_dao->deleteById(queryContext, tempPasswordId);
    return nx::sql::DBResult::ok;
}

boost::optional<const data::TemporaryAccountCredentialsEx&>
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
    const data::TemporaryAccountCredentialsEx& temporaryCredentials)
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
        [](data::TemporaryAccountCredentialsEx& value) { ++value.useCount; });

    // TODO: #ak prolonging expiration period if present.
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
                data::TemporaryAccountCredentialsEx& temporaryAccountCredentials)
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

} // namespace nx::cdb
