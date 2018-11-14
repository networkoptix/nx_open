#pragma once

#include <map>

#include <nx/utils/thread/mutex.h>

#include "../abstract_user_authentication_data_object.h"

namespace nx::cloud::db {
namespace dao {
namespace memory {

class UserAuthentication:
    public AbstractUserAuthentication
{
public:
    UserAuthentication() = default;
    UserAuthentication(const UserAuthentication&) = delete;

    virtual boost::optional<std::string> fetchSystemNonce(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual void insertOrReplaceSystemNonce(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& nonce) override;

    virtual api::AuthInfo fetchUserAuthRecords(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId) override;

    virtual std::vector<std::string> fetchSystemsWithExpiredAuthRecords(
        nx::sql::QueryContext* const queryContext,
        int systemCountLimit) override;

    virtual void insertUserAuthRecords(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId,
        const api::AuthInfo& userAuthRecords) override;

    virtual std::vector<SystemInfo> fetchAccountSystems(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountId) override;

    virtual void deleteAccountAuthRecords(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountId) override;

    virtual void deleteSystemAuthRecords(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId) override;

private:
    std::map<std::string, std::string> m_systemIdToNonce;
    // map<pair<systemId, accountId>, auth info>
    std::map<std::pair<std::string, std::string>, api::AuthInfo> m_userAuthInfo;
    mutable QnMutex m_mutex;
};

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
