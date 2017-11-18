#pragma once

#include <nx/utils/db/db_instance_controller.h>

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

class DbController:
    public nx::utils::db::InstanceController
{
    using base_type = nx::utils::db::InstanceController;

public:
    DbController(const nx::utils::db::ConnectionOptions& connectionOptions);

    using base_type::initialize;
};

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
