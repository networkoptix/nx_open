#pragma once

#include <nx/sql/db_instance_controller.h>

namespace nx::analytics::db {

class DbController:
    public nx::sql::InstanceController
{
    using base_type = nx::sql::InstanceController;

public:
    DbController(const nx::sql::ConnectionOptions& connectionOptions);

    using base_type::initialize;
};

} // namespace nx::analytics::db
