#pragma once

#include <memory>

#include <utils/db/db_instance_controller.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class DbInstanceController:
    public nx::db::InstanceController
{
public:
    DbInstanceController(const nx::db::ConnectionOptions& dbConnectionOptions);
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
