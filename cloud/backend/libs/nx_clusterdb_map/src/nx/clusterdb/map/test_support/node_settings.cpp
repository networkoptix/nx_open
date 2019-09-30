#include "node_settings.h"

namespace nx::clusterdb::map::test {

NodeSettings::NodeSettings():
    base_type("nx", "clusterdbmap service", "clusterdb::map node")
{
}

void NodeSettings::loadSettings()
{
    base_type::loadSettings();
    map.load(settings(), "");
}

void EmbeddedNodeSettings::loadSettings()
{
    base_type::loadSettings();
    sql.loadFromSettings(settings());
}

}