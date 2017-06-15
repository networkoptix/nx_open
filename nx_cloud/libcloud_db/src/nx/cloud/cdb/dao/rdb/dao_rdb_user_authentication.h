#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include "../abstract_user_authentication_data_object.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class UserAuthentication:
    public AbstractUserAuthentication
{
public:
    virtual boost::optional<std::string> fetchSystemNonce(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual void insertOrReplaceSystemNonce(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& nonce) override;

    virtual api::AuthInfo fetchUserAuthRecords(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId) override;

    virtual void insertUserAuthRecords(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        const std::string& accountId,
        const api::AuthInfo& userAuthRecords) override;
};

} // namespace rdb
} // namespace dao

namespace api {

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (AuthInfoRecord),
    (sql_record))

} // namespace api

} // namespace cdb
} // namespace nx
