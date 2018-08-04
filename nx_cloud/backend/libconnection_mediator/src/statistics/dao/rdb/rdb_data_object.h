#pragma once

#include "../abstract_data_object.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {

class DataObject:
    public AbstractDataObject
{
public:
    virtual nx::sql::DBResult save(
        nx::sql::QueryContext* /*queryContext*/,
        ConnectSession connectionRecord) override;

    virtual nx::sql::DBResult readAllRecords(
        nx::sql::QueryContext* /*queryContext*/,
        std::deque<ConnectSession>* connectionRecords) override;
};

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
