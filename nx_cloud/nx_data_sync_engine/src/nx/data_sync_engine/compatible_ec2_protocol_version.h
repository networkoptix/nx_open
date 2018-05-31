#pragma once

#include <nx_ec/ec_proto_version.h>

namespace nx {
namespace data_sync_engine {

static const int kMinSupportedProtocolVersion = 3024;
static const int kMaxSupportedProtocolVersion = nx_ec::EC2_PROTO_VERSION;

NX_DATA_SYNC_ENGINE_API bool isProtocolVersionCompatible(int version);

} // namespace data_sync_engine
} // namespace nx
