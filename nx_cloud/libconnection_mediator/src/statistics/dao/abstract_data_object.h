#pragma once

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../connection_statistics_info.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {

struct ConnectionRecord
{
    ConnectSession sessionStats;
};

class AbstractDataObject
{
public:
    virtual ~AbstractDataObject() = default;

    virtual nx::db::DBResult save(
        nx::db::QueryContext* /*queryContext*/,
        ConnectionRecord connectionRecord) = 0;
};

} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
