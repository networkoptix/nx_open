#pragma once

#include <utils/db/db_instance_controller.h>

namespace nx {
namespace hpm {
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
} // namespace hpm
} // namespace nx
