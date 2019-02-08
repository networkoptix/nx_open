#pragma once

#include <memory>

#include <nx/sql/db_instance_controller.h>

namespace nx::cloud::db {
namespace dao {
namespace rdb {

class DbInstanceController:
    public nx::sql::InstanceController
{
public:
    DbInstanceController(
        const nx::sql::ConnectionOptions& dbConnectionOptions,
        boost::optional<unsigned int> dbVersionToUpdateTo = boost::none);

    bool isUserAuthRecordsMigrationNeeded() const;

private:
    bool m_userAuthRecordsMigrationNeeded;

    void initializeStructureMigration();
};

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
