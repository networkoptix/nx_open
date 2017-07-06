#pragma once

#include <string>

#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

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
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual void insertOrReplaceSystemNonce(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& nonce) = 0;

    virtual api::AuthInfo fetchUserAuthRecords(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId) = 0;

    virtual void insertUserAuthRecords(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId,
        const api::AuthInfo& userAuthRecords) = 0;

    virtual std::vector<SystemInfo> fetchAccountSystems(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& accountId) = 0;

    virtual void deleteAccountAuthRecords(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& accountId) = 0;
};

} // namespace dao
} // namespace cdb
} // namespace nx
