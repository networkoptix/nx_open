#include "compatible_ec2_protocol_version.h"

namespace nx {
namespace data_sync_engine {

bool isProtocolVersionCompatible(int version)
{
    return (version >= kMinSupportedProtocolVersion)
        && (version <= kMaxSupportedProtocolVersion);
}

} // namespace data_sync_engine
} // namespace nx
