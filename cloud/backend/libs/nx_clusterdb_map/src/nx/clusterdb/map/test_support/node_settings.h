#pragma once

#include <nx/clusterdb/engine/service/settings.h>

#include <nx/clusterdb/map/settings.h>
#include <nx/sql/types.h>

namespace nx::clusterdb::map::test {

class NX_KEY_VALUE_DB_API NodeSettings:
    public nx::clusterdb::engine::Settings
{
    using base_type = nx::clusterdb::engine::Settings;

public:
    map::Settings map;

    NodeSettings();

protected:
    virtual void loadSettings() override;
};

class NX_KEY_VALUE_DB_API EmbeddedNodeSettings:
    public NodeSettings
{
    using base_type = NodeSettings;

public:
    nx::sql::ConnectionOptions sql;

protected:
    virtual void loadSettings() override;
};

} // namespace nx::clusterdb::map::test
