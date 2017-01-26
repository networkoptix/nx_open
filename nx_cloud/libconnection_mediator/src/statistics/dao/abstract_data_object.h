#pragma once

#include <deque>

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../connection_statistics_info.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {

class AbstractDataObject
{
public:
    virtual ~AbstractDataObject() = default;

    virtual nx::db::DBResult save(
        nx::db::QueryContext* /*queryContext*/,
        ConnectSession connectionRecord) = 0;

    virtual nx::db::DBResult readAllRecords(
        nx::db::QueryContext* /*queryContext*/,
        std::deque<ConnectSession>* connectionRecords) = 0;
};

} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
