#pragma once

#include "../abstract_data_object.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace memory {

class InMemoryDataObject:
    public AbstractDataObject
{
public:
    virtual nx::db::DBResult save(
        nx::db::QueryContext* /*queryContext*/,
        ConnectionRecord connectionRecord) override;

    const std::vector<ConnectionRecord>& records() const;

private:
    std::vector<ConnectionRecord> m_records;
};

} // namespace memory
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
