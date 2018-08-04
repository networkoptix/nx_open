#include "in_memory_data_object.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace memory {

nx::sql::DBResult InMemoryDataObject::save(
    nx::sql::QueryContext* /*queryContext*/,
    ConnectSession connectionRecord)
{
    m_records.push_back(connectionRecord);
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult InMemoryDataObject::readAllRecords(
    nx::sql::QueryContext* /*queryContext*/,
    std::deque<ConnectSession>* connectionRecords)
{
    *connectionRecords = m_records;
    return nx::sql::DBResult::ok;
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
