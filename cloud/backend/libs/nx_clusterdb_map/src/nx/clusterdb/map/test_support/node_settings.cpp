#include "node_settings.h"

namespace nx::clusterdb::map::test {

NodeSettings::NodeSettings():
    base_type("nx", "clusterdbmap service", "clusterdbmap node")
{
}

void NodeSettings::loadSettings()
{
    base_type::loadSettings();
    map.load(settings());
}

}