#pragma once

#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include <data/system_data.h>

namespace nx {
namespace cdb {
namespace persistent_layer {

class SystemSharingController
{
public:
    nx::db::DBResult insertOrReplaceSharing(
        nx::db::QueryContext* const queryContext,
        const api::SystemSharingEx& sharing);
};

} // namespace persistent_layer
} // namespace cdb
} // namespace nx
