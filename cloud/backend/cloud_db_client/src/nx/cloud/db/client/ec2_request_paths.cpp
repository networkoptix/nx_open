#include "include/nx/cloud/db/api/ec2_request_paths.h"

namespace nx::cloud::db::api {

const char* const kEc2EventsPath = "/cdb/ec2/events";
const char* const kPushEc2TransactionPath = "/cdb/ec2/forward_events";

} // namespace nx::cloud::db::api
