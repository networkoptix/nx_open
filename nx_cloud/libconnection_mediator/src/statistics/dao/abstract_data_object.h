#pragma once

#include <deque>

#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../connection_statistics_info.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {

class AbstractDataObject
{
public:
    virtual ~AbstractDataObject() = default;

    virtual nx::sql::DBResult save(
        nx::sql::QueryContext* /*queryContext*/,
        ConnectSession connectionRecord) = 0;

    virtual nx::sql::DBResult readAllRecords(
        nx::sql::QueryContext* /*queryContext*/,
        std::deque<ConnectSession>* connectionRecords) = 0;
};

} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
