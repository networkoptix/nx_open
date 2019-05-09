#pragma once

#include <nx/utils/std/optional.h>
#include <nx/utils/url.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <nx/cloud/relay/relay_service.h>

#include "remote_relay_peer_pool.h"

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
    enum class Mode
    {
        singleRelay,
        cluster,
    };

    BasicComponentTest(Mode mode = Mode::cluster);
    ~BasicComponentTest();

    void addRelayInstance(
        std::vector<const char*> args = {},
        bool waitUntilStarted = true);

    Relay& relay(int index = 0);
    const Relay& relay(int index = 0) const;

    void stopAllInstances();

    /**
     * @return true, if information about peer is available to every relay peer.
     */
    bool peerInformationSynchronizedInCluster(const std::string& hostname);

private:
    ListeningPeerPool m_listeningPeerPool;
    std::vector<std::unique_ptr<Relay>> m_relays;
    std::optional<model::RemoteRelayPeerPoolFactory::Function> m_factoryFunctionBak;

    std::unique_ptr<model::AbstractRemoteRelayPeerPool> createRemoteRelayPeerPool(
        const conf::Settings& settings);
};

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
