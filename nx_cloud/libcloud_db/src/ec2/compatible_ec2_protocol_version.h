#pragma once

#include <nx_ec/ec_proto_version.h>

namespace nx {
namespace cdb {
namespace ec2 {

static const int kMinSupportProtocolVersion = 3024;
static const int kMaxSupportProtocolVersion = nx_ec::EC2_PROTO_VERSION;

bool isProtocolVersionCompatible(int version);

} // namespace ec2
} // namespace cdb
} // namespace nx
