#include "server_statistics.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace network {
namespace server {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Statistics),
    (json),
    _server_Fields)

} // namespace server
} // namespace network
} // namespace nx
