#include "listening_peer.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace hpm {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ListeningPeer)(BoundClient)(ListeningPeers),
    (json),
    _Fields);

} // namespace api
} // namespace hpm
} // namespace nx
