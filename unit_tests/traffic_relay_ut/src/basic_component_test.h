#pragma once

#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <nx/cloud/relay/relay_service.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class BasicComponentTest:
    public utils::test::ModuleLauncher<RelayService>,
    public utils::test::TestWithTemporaryDirectory
{
public:
    BasicComponentTest(QString tmpDir = QString());
    ~BasicComponentTest();
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
