#include "dao_rdb_user_authentication.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

std::string UserAuthentication::fetchSystemNonce(
    nx::db::QueryContext* const /*queryContext*/,
    const std::string& /*systemId*/)
{
    // TODO
    return std::string();
}

void UserAuthentication::insertOrReplaceSystemNonce(
    nx::db::QueryContext* const /*queryContext*/,
    const std::string& /*systemId*/,
    const std::string& /*nonce*/)
{
    // TODO
}

std::vector<api::AuthInfo> UserAuthentication::fetchUserAuthRecords(
    nx::db::QueryContext* const /*queryContext*/,
    const std::string& /*systemId*/,
    const std::string& /*userEmail*/)
{
    // TODO
    return std::vector<api::AuthInfo>();
}

void UserAuthentication::saveUserAuthRecords(
    nx::db::QueryContext* const /*queryContext*/,
    const std::string& /*systemId*/,
    const std::string& /*accountEmail*/,
    const std::vector<api::AuthInfo>& /*userAuthRecords*/)
{
    // TODO
}

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
