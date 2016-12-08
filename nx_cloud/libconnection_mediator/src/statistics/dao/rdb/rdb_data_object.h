#pragma once

#include "../abstract_data_object.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {

class RdbDataObject:
    public AbstractDataObject
{
public:
    virtual nx::db::DBResult save(
        nx::db::QueryContext* /*queryContext*/,
        ConnectionRecord connectionRecord) override;
};

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
