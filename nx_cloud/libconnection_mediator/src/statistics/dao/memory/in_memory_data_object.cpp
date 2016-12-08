#include "in_memory_data_object.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace memory {

nx::db::DBResult InMemoryDataObject::save(
    nx::db::QueryContext* /*queryContext*/,
    ConnectionRecord connectionRecord)
{
    m_records.push_back(connectionRecord);
    return nx::db::DBResult::ok;
}

const std::vector<ConnectionRecord>& InMemoryDataObject::records() const
{
    return m_records;
}

} // namespace memory
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
