#pragma once

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "data/account_data.h"

namespace nx {
namespace cdb {
namespace persistent_layer {

class AccountController
{
public:
    nx::db::DBResult insert(
        nx::db::QueryContext* queryContext,
        const api::AccountData& account);
};

} // namespace persistent_layer
} // namespace cdb
} // namespace nx
