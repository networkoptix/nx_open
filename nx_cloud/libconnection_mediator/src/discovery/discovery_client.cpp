#include "discovery_client.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace cloud {
namespace discovery {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (BasicInstanceInformation),
    (json),
    _Fields)

} // namespace nx
} // namespace cloud
} // namespace discovery

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::discovery, ResultCode,
    (nx::cloud::discovery::ResultCode::ok, "ok")
    (nx::cloud::discovery::ResultCode::notFound, "notFound")
    (nx::cloud::discovery::ResultCode::networkError, "networkError")
    (nx::cloud::discovery::ResultCode::invalidModuleInformation, "invalidModuleInformation")
)

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::discovery, PeerStatus,
    (nx::cloud::discovery::PeerStatus::online, "online")
    (nx::cloud::discovery::PeerStatus::offline, "offline")
)
