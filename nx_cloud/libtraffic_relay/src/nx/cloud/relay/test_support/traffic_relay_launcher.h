#pragma once

#include <nx/utils/test_support/module_instance_launcher.h>

#include "../relay_service.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class Launcher:
    public nx::utils::test::ModuleLauncher<RelayService>
{
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
