#include "ping.h"

#include <nx/cloud/db/client/cdb_request_path.h>

namespace nx::cloud::db {
namespace http_handler {

const char* Ping::kHandlerPath = kPingPath;

} // namespace http_handler
} // namespace nx::cloud::db
