#pragma once

#include "../abstract_user_authentication_data_object.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class UserAuthentication:
    public AbstractUserAuthentication
{
public:
    virtual std::string fetchSystemNonce(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual void insertOrReplaceSystemNonce(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& nonce) override;

    virtual api::AuthInfo fetchUserAuthRecords(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& userEmail) override;

    virtual void saveUserAuthRecords(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountEmail,
        const api::AuthInfo& userAuthRecords) override;
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
