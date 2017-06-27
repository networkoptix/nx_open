#pragma once

#include <memory>

#include <nx/utils/db/db_instance_controller.h>

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class DbInstanceController:
    public nx::utils::db::InstanceController
{
public:
    DbInstanceController(
        const nx::utils::db::ConnectionOptions& dbConnectionOptions,
        boost::optional<unsigned int> dbVersionToUpdateTo = boost::none);

    bool isUserAuthRecordsMigrationNeeded() const;

private:
    bool m_userAuthRecordsMigrationNeeded;

    void initializeStructureMigration();
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
