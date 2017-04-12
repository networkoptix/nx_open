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
    virtual nx::db::DBResult save(
        nx::db::QueryContext* /*queryContext*/,
        ConnectSession connectionRecord) override;

    virtual nx::db::DBResult readAllRecords(
        nx::db::QueryContext* /*queryContext*/,
        std::deque<ConnectSession>* connectionRecords) override;
};

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
