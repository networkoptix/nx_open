#pragma once

#include <chrono>

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../../data/account_data.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class AccountDataObject
{
public:
    nx::db::DBResult insert(
        nx::db::QueryContext* queryContext,
        const api::AccountData& account);

    nx::db::DBResult update(
        nx::db::QueryContext* queryContext,
        const api::AccountData& account);

    nx::db::DBResult fetchAccounts(
        nx::db::QueryContext* queryContext,
        std::vector<data::AccountData>* accounts);

    nx::db::DBResult getAccountEmailByVerificationCode(
        nx::db::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode,
        std::string* accountEmail);

    nx::db::DBResult removeVerificationCode(
        nx::db::QueryContext* queryContext,
        const data::AccountConfirmationCode& verificationCode);

    nx::db::DBResult updateAccountToActiveStatus(
        nx::db::QueryContext* queryContext,
        const std::string& accountEmail,
        std::chrono::system_clock::time_point activationTime);
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
