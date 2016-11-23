#pragma once

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "data/account_data.h"

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
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
