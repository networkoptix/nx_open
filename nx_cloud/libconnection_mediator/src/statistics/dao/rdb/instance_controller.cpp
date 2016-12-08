#include "instance_controller.h"

#include "db_schema_scripts.h"

namespace nx {
namespace hpm {
namespace dao {
namespace rdb {

InstanceController::InstanceController(const nx::db::ConnectionOptions& connectionOptions):
    nx::db::InstanceController(connectionOptions)
{
    dbStructureUpdater().addUpdateScript(kCreateConnectSessionStatisticsTable);
}

} // namespace rdb
} // namespace dao
} // namespace hpm
} // namespace nx
