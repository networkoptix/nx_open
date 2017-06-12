#pragma once

#include <string>

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include <cdb/auth_provider.h>

namespace nx {
namespace cdb {
namespace dao {

/**
 * @throw Every method throws nx:db::Exception on error.
 */
class AbstractUserAuthentication
{
public:
    virtual ~AbstractUserAuthentication() = default;

    virtual std::string fetchSystemNonce(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual void insertOrReplaceSystemNonce(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& nonce) = 0;

    virtual api::AuthInfo fetchUserAuthRecords(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& userEmail) = 0;

    virtual void saveUserAuthRecords(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountEmail,
        const api::AuthInfo& userAuthRecords) = 0;
};

} // namespace dao
} // namespace cdb
} // namespace nx
