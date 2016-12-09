#pragma once

#include <utils/db/db_instance_controller.h>

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {

class InstanceController:
    public nx::db::InstanceController
{
public:
    InstanceController(const nx::db::ConnectionOptions& connectionOptions);
};

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
