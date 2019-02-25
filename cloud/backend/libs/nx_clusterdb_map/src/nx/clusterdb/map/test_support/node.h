#pragma once

#include <nx/clusterdb/engine/service/service.h>

#include "nx/clusterdb/map/database.h"

namespace nx::clusterdb::map::test {

class NX_KEY_VALUE_DB_API Node:
    public nx::clusterdb::engine::Service
{
public:
    Node(int argc, char** argv);

    map::Database& database();

protected:
    virtual void setup() override;
    virtual void teardown() override;

private:
    std::unique_ptr<map::Database> m_database;
};

} // namespace nx::clusterdb::map::test