#include "dao_memory_user_authentication.h"

#include <utility>

namespace nx {
namespace cdb {
namespace dao {
namespace memory {

boost::optional<std::string> UserAuthentication::fetchSystemNonce(
    nx::db::QueryContext* const /*queryContext*/,
    const std::string& systemId)
{
    auto it = m_systemIdToNonce.find(systemId);
    if (it == m_systemIdToNonce.end())
        return boost::none;
    return it->second;
}

void UserAuthentication::insertOrReplaceSystemNonce(
    nx::db::QueryContext* const /*queryContext*/,
    const std::string& systemId,
    const std::string& nonce)
{
    m_systemIdToNonce.emplace(systemId, nonce);
}

api::AuthInfo UserAuthentication::fetchUserAuthRecords(
    nx::db::QueryContext* const /*queryContext*/,
    const std::string& systemId,
    const std::string& userEmail)
{
    auto it = m_userAuthInfo.find(std::make_pair(systemId, userEmail));
    if (it == m_userAuthInfo.end())
        return api::AuthInfo();

    return it->second;
}

void UserAuthentication::insertUserAuthRecords(
    nx::db::QueryContext* const /*queryContext*/,
    const std::string& systemId,
    const std::string& accountEmail,
    const api::AuthInfo& userAuthRecords)
{
    m_userAuthInfo[std::make_pair(systemId, accountEmail)] = userAuthRecords;
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
