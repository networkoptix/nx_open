#pragma once

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include <data/system_data.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class SystemSharingDataObject
{
public:
    nx::db::DBResult insertOrReplaceSharing(
        nx::db::QueryContext* const queryContext,
        const api::SystemSharingEx& sharing);
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
