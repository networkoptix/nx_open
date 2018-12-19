#include "peer_info.h"

#include <nx/fusion/model_functions.h>

namespace nx::clusterdb::engine::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (PeerInfo),
    (json),
    _Fields)

} // namespace nx::clusterdb::engine::api
