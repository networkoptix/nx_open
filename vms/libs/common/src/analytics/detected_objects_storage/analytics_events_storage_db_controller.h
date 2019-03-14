#pragma once

#include <nx/sql/db_instance_controller.h>

namespace nx {
namespace analytics {
namespace storage {

class DbController:
    public nx::sql::InstanceController
{
    using base_type = nx::sql::InstanceController;

public:
    DbController(const nx::sql::ConnectionOptions& connectionOptions);

    using base_type::initialize;
};

} // namespace storage
} // namespace analytics
} // namespace nx
