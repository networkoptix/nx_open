#include "ping.h"

#include <nx/cloud/cdb/client/cdb_request_path.h>

namespace nx::cloud::db {
namespace http_handler {

const QString Ping::kHandlerPath = QLatin1String(kPingPath);

} // namespace http_handler
} // namespace nx::cloud::db
