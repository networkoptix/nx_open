#pragma once

#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <nx/cloud/relay/relay_service.h>

namespace nx {
namespace cloud {
namespace relay {
namespace test {

class Relay:
    public utils::test::ModuleLauncher<relay::RelayService>
{
public:
    Relay();
    ~Relay();

    nx::utils::Url basicUrl() const;
};

//-------------------------------------------------------------------------------------------------

class BasicComponentTest:
    public utils::test::TestWithTemporaryDirectory
{
public:
    BasicComponentTest(QString tmpDir = QString());

    void addRelayInstance(
        std::vector<const char*> args = {},
        bool waitUntilStarted = true);

    Relay& relay(int index = 0);
    const Relay& relay(int index = 0) const;

    void stopAllInstances();

private:
    std::vector<std::unique_ptr<Relay>> m_relays;
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
