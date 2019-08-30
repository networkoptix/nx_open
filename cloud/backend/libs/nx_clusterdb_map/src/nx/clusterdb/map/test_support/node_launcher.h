#pragma once

#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include "node.h"

namespace nx::clusterdb::map::test {

class NX_KEY_VALUE_DB_API NodeLauncher:
    public nx::utils::test::ModuleLauncher<Node>
{
public:
    NodeLauncher();
    ~NodeLauncher();
};

//-------------------------------------------------------------------------------------------------
// EmbeddedNodeLauncher

class NX_KEY_VALUE_DB_API EmbeddedNodeLauncher:
    public nx::utils::test::ModuleLauncher<EmbeddedNode>,
    public nx::utils::test::TestWithTemporaryDirectory
{
    using base_type = nx::utils::test::TestWithTemporaryDirectory;

public:
    EmbeddedNodeLauncher();
    ~EmbeddedNodeLauncher();
};

} // namespace nx::clusterdb::map::test