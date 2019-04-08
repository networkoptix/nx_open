#pragma once

#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/mediator/api/listening_peer.h>
#include <nx/network/socket_common.h>
#include <nx/network/stream_proxy.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include "../cloud_data_provider.h"
#include "../mediator_process_public.h"
#include "local_cloud_data_provider.h"
#include "mediaserver_emulator.h"

namespace nx {
namespace hpm {

namespace ServerTweak {
enum Value
{
    defaultBehavior = 0,
    noBindEndpoint = 1 << 0,
    noListenToConnect = 1 << 1,
};
} // namespace ServerBehavior

class MediatorFunctionalTest:
    public utils::test::ModuleLauncher<MediatorProcessPublic>,
    public utils::test::TestWithTemporaryDirectory
{
public:
    enum MediatorTestFlags
    {
        noFlags = 0,
        useTestCloudDataProvider = 0x01,
        initializeSocketGlobals = 0x02,
        initializeConnectivity = 0x04,
        allFlags = useTestCloudDataProvider | initializeSocketGlobals | initializeConnectivity,
    };

    //!Calls \a start
    MediatorFunctionalTest(int flags = allFlags, const QString& testDir = QString());
    ~MediatorFunctionalTest();

    /**
     * Use it to make restart reliable.
     * Without using proxy mediator is not able to automatically attach to the same port after
     * restart.
     * false by default.
     */
    void setUseProxy(bool value);

    virtual bool waitUntilStarted() override;

    network::SocketAddress stunUdpEndpoint() const;
    network::SocketAddress stunTcpEndpoint() const;
    network::SocketAddress httpEndpoint() const;
    nx::utils::Url httpUrl() const;

    std::unique_ptr<nx::hpm::api::MediatorClientTcpConnection> clientConnection();
    std::unique_ptr<nx::hpm::api::MediatorServerTcpConnection> systemConnection();

    void registerCloudDataProvider(AbstractCloudDataProvider* cloudDataProvider);

    AbstractCloudDataProvider::System addRandomSystem();

    std::unique_ptr<MediaServerEmulator> addServer(
        const AbstractCloudDataProvider::System& system,
        nx::String name, ServerTweak::Value tweak = ServerTweak::defaultBehavior);

    std::unique_ptr<MediaServerEmulator> addRandomServer(
        const AbstractCloudDataProvider::System& system,
        boost::optional<QnUuid> serverId = boost::none,
        ServerTweak::Value tweak = ServerTweak::defaultBehavior);

    std::vector<std::unique_ptr<MediaServerEmulator>> addRandomServers(
        const AbstractCloudDataProvider::System& system,
        size_t count, ServerTweak::Value tweak = ServerTweak::defaultBehavior);

    std::tuple<api::ResultCode, api::ListeningPeers>
        getListeningPeers() const;

protected:
    virtual void beforeModuleCreation() override;
    virtual void afterModuleDestruction() override;

private:
    struct TcpProxyContext
    {
        std::unique_ptr<nx::network::StreamProxy> server;
        nx::network::SocketAddress endpoint;
    };

    const int m_testFlags;
    LocalCloudDataProvider m_cloudDataProvider;
    boost::optional<AbstractCloudDataProviderFactory::FactoryFunc> m_factoryFuncToRestore;
    std::optional<network::SocketAddress> m_stunUdpEndpoint;
    network::SocketAddress m_stunTcpEndpoint;
    network::SocketAddress m_httpEndpoint;
    TcpProxyContext m_httpProxy;
    TcpProxyContext m_stunProxy;
    bool m_tcpPortsAllocated = false;
    bool m_useProxy = false;

    int m_startCount = 0;

    bool startProxy();
    bool allocateTcpPorts();
};

} // namespace hpm
} // namespace nx
