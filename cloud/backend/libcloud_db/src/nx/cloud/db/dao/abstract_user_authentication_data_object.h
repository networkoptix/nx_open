#pragma once

#include <string>

#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include <nx/cloud/db/api/auth_provider.h>

namespace nx::cloud::db {
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
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual void insertOrReplaceSystemNonce(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& nonce) = 0;

    virtual api::AuthInfo fetchUserAuthRecords(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId) = 0;

    /**
     * @return System id list.
     */
    virtual std::vector<std::string> fetchSystemsWithExpiredAuthRecords(
        nx::sql::QueryContext* const queryContext,
        int systemCountLimit) = 0;

    virtual void insertUserAuthRecords(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId,
        const api::AuthInfo& userAuthRecords) = 0;

    virtual std::vector<SystemInfo> fetchAccountSystems(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountId) = 0;

    virtual void deleteAccountAuthRecords(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountId) = 0;

    virtual void deleteSystemAuthRecords(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId) = 0;
};

} // namespace dao
} // namespace nx::cloud::db
