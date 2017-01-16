#pragma once

#include <deque>

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
        ConnectSession connectionRecord) override;

    virtual nx::db::DBResult readAllRecords(
        nx::db::QueryContext* /*queryContext*/,
        std::deque<ConnectSession>* connectionRecords) override;

    const std::deque<ConnectSession>& records() const;

private:
    std::deque<ConnectSession> m_records;
};

} // namespace memory
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
