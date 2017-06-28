#pragma once

#include <nx/utils/db/db_instance_controller.h>

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {

class InstanceController:
    public nx::utils::db::InstanceController
{
public:
    InstanceController(const nx::utils::db::ConnectionOptions& connectionOptions);
};

} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
