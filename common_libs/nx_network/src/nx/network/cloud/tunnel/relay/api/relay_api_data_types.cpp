#include "relay_api_data_types.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (BeginListeningRequest)(BeginListeningResponse)(CreateClientSessionRequest) \
        (CreateClientSessionResponse)(ConnectToPeerRequest),
    (json),
    _Fields)

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
