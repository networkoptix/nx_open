#pragma once

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "data/system_data.h"

namespace nx {
namespace cdb {
namespace persistent_layer {

class SystemController
{
public:
    nx::db::DBResult insert(
        nx::db::QueryContext* const queryContext,
        const data::SystemData& system,
        const std::string& accountId);

    nx::db::DBResult selectSystemSequence(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        std::uint64_t* const sequence);
};

} // namespace persistent_layer
} // namespace cdb
} // namespace nx
