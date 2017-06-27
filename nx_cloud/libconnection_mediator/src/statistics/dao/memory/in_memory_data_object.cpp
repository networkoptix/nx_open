#include "in_memory_data_object.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace memory {

nx::utils::db::DBResult InMemoryDataObject::save(
    nx::utils::db::QueryContext* /*queryContext*/,
    ConnectSession connectionRecord)
{
    m_records.push_back(connectionRecord);
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult InMemoryDataObject::readAllRecords(
    nx::utils::db::QueryContext* /*queryContext*/,
    std::deque<ConnectSession>* connectionRecords)
{
    *connectionRecords = m_records;
    return nx::utils::db::DBResult::ok;
}

const std::deque<ConnectSession>& InMemoryDataObject::records() const
{
    return m_records;
}

} // namespace memory
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
