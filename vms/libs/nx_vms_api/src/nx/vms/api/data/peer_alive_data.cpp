#include "peer_alive_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (PeerAliveData),
    (eq)(ubjson)(json)(xml),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
