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
    virtual nx::utils::db::DBResult save(
        nx::utils::db::QueryContext* /*queryContext*/,
        ConnectSession connectionRecord) override;

    virtual nx::utils::db::DBResult readAllRecords(
        nx::utils::db::QueryContext* /*queryContext*/,
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
