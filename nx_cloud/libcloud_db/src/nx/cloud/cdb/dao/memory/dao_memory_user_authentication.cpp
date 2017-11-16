#include "dao_memory_user_authentication.h"

#include <set>
#include <utility>

namespace nx {
namespace cdb {
namespace dao {
namespace memory {

boost::optional<std::string> UserAuthentication::fetchSystemNonce(
    nx::utils::db::QueryContext* const /*queryContext*/,
    const std::string& systemId)
{
    auto it = m_systemIdToNonce.find(systemId);
    if (it == m_systemIdToNonce.end())
        return boost::none;
    return it->second;
}

void UserAuthentication::insertOrReplaceSystemNonce(
    nx::utils::db::QueryContext* const /*queryContext*/,
    const std::string& systemId,
    const std::string& nonce)
{
    m_systemIdToNonce[systemId] = nonce;
}

api::AuthInfo UserAuthentication::fetchUserAuthRecords(
    nx::utils::db::QueryContext* const /*queryContext*/,
    const std::string& systemId,
    const std::string& accountId)
{
    auto it = m_userAuthInfo.find(std::make_pair(systemId, accountId));
    if (it == m_userAuthInfo.end())
        return api::AuthInfo();

    return it->second;
}

void UserAuthentication::insertUserAuthRecords(
    nx::utils::db::QueryContext* const /*queryContext*/,
    const std::string& systemId,
    const std::string& accountId,
    const api::AuthInfo& userAuthRecords)
{
    m_userAuthInfo[std::make_pair(systemId, accountId)] = userAuthRecords;
}

std::vector<AbstractUserAuthentication::SystemInfo>
    UserAuthentication::fetchAccountSystems(
        nx::utils::db::QueryContext* const /*queryContext*/,
        const std::string& accountId)
{
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
    nx::utils::db::QueryContext* const /*queryContext*/,
    const std::string& accountId)
{
    for (auto it = m_userAuthInfo.begin(); it != m_userAuthInfo.end(); )
    {
        if (it->first.second == accountId)
            it = m_userAuthInfo.erase(it);
        else
            ++it;
    }
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
