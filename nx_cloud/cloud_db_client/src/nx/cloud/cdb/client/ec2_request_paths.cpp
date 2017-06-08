#include "include/nx/cloud/cdb/api/ec2_request_paths.h"

namespace nx {
namespace cdb {
namespace api {

const char* const kEc2EventsPath = "/cdb/ec2/events";
const char* const kPushEc2TransactionPath = "/cdb/ec2/forward_events";
const char* const kDeprecatedPushEc2TransactionPath = "/ec2/forward_events";

} // namespace api
} // namespace cdb
} // namespace nx
