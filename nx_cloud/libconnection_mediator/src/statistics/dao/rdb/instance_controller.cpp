#include "instance_controller.h"

#include "db_schema_scripts.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {

InstanceController::InstanceController(const nx::db::ConnectionOptions& connectionOptions):
    nx::db::InstanceController(connectionOptions)
{
    dbStructureUpdater().addUpdateScript(kCreateConnectSessionStatisticsTable);

    if (!initialize())
        throw std::runtime_error("Could not connect to DB");
}

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
