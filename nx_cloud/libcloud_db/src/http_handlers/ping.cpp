#include "ping.h"

#include <cloud_db_client/src/cdb_request_path.h>

namespace nx {
namespace cdb {
namespace http_handler {

const QString Ping::kHandlerPath = QLatin1String(kPingPath);

} // namespace http_handler
} // namespace cdb
} // namespace nx
