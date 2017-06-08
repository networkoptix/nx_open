#pragma once

#include <nx/cloud/cdb/api/maintenance_manager.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>

namespace nx {
namespace cdb {
namespace api {

#define VmsConnectionData_Fields (systemId)(mediaserverEndpoint)
#define VmsConnectionDataList_Fields (connections)

#define Statistics_Fields (onlineServerCount)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (VmsConnectionData)(VmsConnectionDataList)(Statistics),
    (json));

} // namespace api
} // namespace cdb
} // namespace nx
