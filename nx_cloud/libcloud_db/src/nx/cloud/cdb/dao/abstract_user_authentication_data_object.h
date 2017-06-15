#pragma once

#include <string>

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include <nx/cloud/cdb/api/auth_provider.h>

namespace nx {
namespace cdb {
namespace dao {

/**
 * @throw Every method throws nx:db::Exception on error.
 */
class AbstractUserAuthentication
{
public:
    struct SystemInfo
    {
        std::string systemId;
        std::string nonce;
        std::string vmsUserId;
    };

    virtual ~AbstractUserAuthentication() = default;

    virtual boost::optional<std::string> fetchSystemNonce(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual void insertOrReplaceSystemNonce(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& nonce) = 0;

    virtual api::AuthInfo fetchUserAuthRecords(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId) = 0;

    virtual void insertUserAuthRecords(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId,
        const api::AuthInfo& userAuthRecords) = 0;

    virtual std::vector<SystemInfo> fetchAccountSystems(
        nx::db::QueryContext* const queryContext,
        const std::string& accountId) = 0;

    virtual void deleteAccountAuthRecords(
        nx::db::QueryContext* const queryContext,
        const std::string& accountId) = 0;
};

} // namespace dao
} // namespace cdb
} // namespace nx
