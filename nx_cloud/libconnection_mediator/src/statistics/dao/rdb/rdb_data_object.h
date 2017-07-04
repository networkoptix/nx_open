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
    virtual nx::utils::db::DBResult save(
        nx::utils::db::QueryContext* /*queryContext*/,
        ConnectSession connectionRecord) override;

    virtual nx::utils::db::DBResult readAllRecords(
        nx::utils::db::QueryContext* /*queryContext*/,
        std::deque<ConnectSession>* connectionRecords) override;
};

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
