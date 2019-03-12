#pragma once

#include <nx/utils/test_support/module_instance_launcher.h>

#include "node.h"

namespace nx::clusterdb::map::test {

class NX_KEY_VALUE_DB_API NodeLauncher:
    public nx::utils::test::ModuleLauncher<Node>
{
public:
    NodeLauncher();
    ~NodeLauncher();
};

} // namespace nx::clusterdb::map::test