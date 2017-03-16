#include "compatible_ec2_protocol_version.h"

namespace nx {
namespace cdb {
namespace ec2 {

bool isProtocolVersionCompatible(int version)
{
    return (version >= kMinSupportProtocolVersion)
        && (version <= kMaxSupportProtocolVersion);
}

} // namespace ec2
} // namespace cdb
} // namespace nx
