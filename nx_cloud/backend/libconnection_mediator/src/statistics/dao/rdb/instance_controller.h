#pragma once

#include <nx/sql/db_instance_controller.h>

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {

class InstanceController:
    public nx::sql::InstanceController
{
public:
    InstanceController(const nx::sql::ConnectionOptions& connectionOptions);
};

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
