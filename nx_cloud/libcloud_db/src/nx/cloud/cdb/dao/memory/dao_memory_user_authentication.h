#pragma once

#include <map>

#include "../abstract_user_authentication_data_object.h"

namespace nx {
namespace cdb {
namespace dao {
namespace memory {

class UserAuthentication:
    public AbstractUserAuthentication
{
public:
    UserAuthentication() = default;
    UserAuthentication(const UserAuthentication&) = delete;

    virtual boost::optional<std::string> fetchSystemNonce(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual void insertOrReplaceSystemNonce(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& nonce) override;

    virtual api::AuthInfo fetchUserAuthRecords(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId) override;

    virtual void insertUserAuthRecords(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId,
        const api::AuthInfo& userAuthRecords) override;

    virtual std::vector<SystemInfo> fetchAccountSystems(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& accountId) override;

    virtual void deleteAccountAuthRecords(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& accountId) override;

private:
    std::map<std::string, std::string> m_systemIdToNonce;
    // map<pair<systemId, accountId>, auth info>
    std::map<std::pair<std::string, std::string>, api::AuthInfo> m_userAuthInfo;
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
