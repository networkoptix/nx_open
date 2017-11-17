#pragma once

#include <nx/utils/db/db_instance_controller.h>

namespace nx {
namespace mediaserver {
namespace analytics {
namespace storage {

class InstanceController:
    public nx::utils::db::InstanceController
{
    using base_type = nx::utils::db::InstanceController;

public:
    InstanceController(const nx::utils::db::ConnectionOptions& connectionOptions);
};

} // namespace storage
} // namespace analytics
} // namespace mediaserver
} // namespace nx
