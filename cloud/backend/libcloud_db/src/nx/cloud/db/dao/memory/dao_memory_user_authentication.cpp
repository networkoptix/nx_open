#include "dao_memory_user_authentication.h"

#include <set>
#include <utility>

#include <nx/utils/time.h>

namespace nx::cloud::db {
namespace dao {
namespace memory {

boost::optional<std::string> UserAuthentication::fetchSystemNonce(
    nx::sql::QueryContext* const /*queryContext*/,
    const std::string& systemId)
{
    QnMutexLocker locker(&m_mutex);

    auto it = m_systemIdToNonce.find(systemId);
    if (it == m_systemIdToNonce.end())
        return boost::none;
    return it->second;
}

void UserAuthentication::insertOrReplaceSystemNonce(
    nx::sql::QueryContext* const /*queryContext*/,
    const std::string& systemId,
    const std::string& nonce)
{
    QnMutexLocker locker(&m_mutex);

    m_systemIdToNonce[systemId] = nonce;
}

api::AuthInfo UserAuthentication::fetchUserAuthRecords(
    nx::sql::QueryContext* const /*queryContext*/,
    const std::string& systemId,
    const std::string& accountId)
{
    QnMutexLocker locker(&m_mutex);

    auto it = m_userAuthInfo.find(std::make_pair(systemId, accountId));
    if (it == m_userAuthInfo.end())
        return api::AuthInfo();

    return it->second;
}

std::vector<std::string> UserAuthentication::fetchSystemsWithExpiredAuthRecords(
    nx::sql::QueryContext* const /*queryContext*/,
    int systemCountLimit)
{
    QnMutexLocker locker(&m_mutex);

    std::set<std::string> systems;

    const auto currentTime = nx::utils::utcTime();

    for (const auto& userAuthInfo: m_userAuthInfo)
    {
        for (const auto& record: userAuthInfo.second.records)
        {
            if (record.expirationTime > currentTime)
                continue;

            systems.insert(userAuthInfo.first.first);
            break;
        }

        if (systems.size() >= (unsigned int)systemCountLimit)
            break;
    }

    return std::vector<std::string>(systems.cbegin(), systems.cend());
}

void UserAuthentication::insertUserAuthRecords(
    nx::sql::QueryContext* const /*queryContext*/,
    const std::string& systemId,
    const std::string& accountId,
    const api::AuthInfo& userAuthRecords)
{
    QnMutexLocker locker(&m_mutex);

    m_userAuthInfo[std::make_pair(systemId, accountId)] = userAuthRecords;
}

std::vector<AbstractUserAuthentication::SystemInfo>
    UserAuthentication::fetchAccountSystems(
        nx::sql::QueryContext* const /*queryContext*/,
        const std::string& accountId)
{
    QnMutexLocker locker(&m_mutex);

    std::vector<SystemInfo> result;
    std::set<std::string> systemsAdded;

    for (const auto& value: m_userAuthInfo)
    {
        if (value.first.second != accountId)
            continue;

        const auto& systemId = value.first.first;
        if (!systemsAdded.insert(systemId).second)
            continue;

        SystemInfo systemInfo;
        systemInfo.systemId = value.first.first;
        //systemInfo.vmsUserId = ;
        systemInfo.nonce = m_systemIdToNonce[systemInfo.systemId];
        result.push_back(std::move(systemInfo));
    }

    return result;
}

void UserAuthentication::deleteAccountAuthRecords(
    nx::sql::QueryContext* const /*queryContext*/,
    const std::string& accountId)
{
    QnMutexLocker locker(&m_mutex);

    for (auto it = m_userAuthInfo.begin(); it != m_userAuthInfo.end(); )
    {
        if (it->first.second == accountId)
            it = m_userAuthInfo.erase(it);
        else
            ++it;
    }
}

void UserAuthentication::deleteSystemAuthRecords(
    nx::sql::QueryContext* const /*queryContext*/,
    const std::string& systemId)
{
    QnMutexLocker locker(&m_mutex);

    for (auto it = m_userAuthInfo.lower_bound({systemId, std::string()});
        it != m_userAuthInfo.end();
        )
    {
        if (it->first.first != systemId)
            break;

        it = m_userAuthInfo.erase(it);
    }
}

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
