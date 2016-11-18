#ifndef NX_NETWORK_SOCKET_GLOBAL_H
#define NX_NETWORK_SOCKET_GLOBAL_H

#include <nx/utils/log/log.h>
#include <nx/utils/singleton.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/flag_config.h>

#include "aio/aioservice.h"

#include "cloud/address_resolver.h"
#include "cloud/cloud_connect_settings.h"
#include "cloud/mediator_address_publisher.h"
#include "cloud/mediator_connector.h"
#include "cloud/tunnel/outgoing_tunnel_pool.h"
#include "cloud/tunnel/tcp/reverse_connection_pool.h"

#define NX_NETWORK_SOCKET_GLOBALS

namespace nx {
namespace network {

class NX_NETWORK_API SocketGlobals
{
public:
    struct NX_NETWORK_API DebugConfiguration: nx::utils::FlagConfig
    {
        DebugConfiguration(): nx::utils::FlagConfig("nx_network_debug") { reload(); }

        NX_FLAG(0, multipleServerSocket, "Extra debug info from MultipleServerSocket");
        NX_FLAG(0, cloudServerSocket, "Extra debug info from cloud::CloudServerSocket");
        NX_FLAG(0, addressResolver, "Extra debug info from cloud::AddressResolver");
    };

    struct NX_NETWORK_API Config: nx::utils::FlagConfig
    {
        Config(): nx::utils::FlagConfig("nx_network") { reload(); }

        NX_FLAG(0, disableCloudSockets, "Use plain TCP sockets instead of Cloud sockets");
        NX_STRING_PARAM("", disableHosts, "Comma separated list of forbidden IPs and domains");

        bool isHostDisabled(const HostAddress& address) const;
    };

    typedef cloud::MediatorAddressPublisher AddressPublisher;
    typedef hpm::api::MediatorConnector MediatorConnector;
    typedef cloud::OutgoingTunnelPool OutgoingTunnelPool;
    typedef cloud::CloudConnectSettings CloudSettings;
    typedef cloud::tcp::ReverseConnectionPool TcpReversePool;

    static Config& config() { return s_instance->m_config; }
    static DebugConfiguration& debugConfiguration() { return s_instance->m_debugConfiguration; }
    static aio::AIOService& aioService() { return s_instance->m_aioService; }
    static cloud::AddressResolver& addressResolver() { return *s_instance->m_addressResolver; }
    static AddressPublisher& addressPublisher() { return s_instance->m_addressPublisher; }
    static MediatorConnector& mediatorConnector() { return *s_instance->m_mediatorConnector; }
    static OutgoingTunnelPool& outgoingTunnelPool() { return s_instance->m_outgoingTunnelPool; }
    static CloudSettings& cloudConnectSettings() { return s_instance->m_cloudConnectSettings; }
    static TcpReversePool& tcpReversePool() { return s_instance->m_tcpReversePool; }

    static void init(); /**< Should be called before any socket use */
    static void deinit(); /**< Should be called when sockets are not needed any more */
    static void verifyInitialization();

    static void applyArguments(const utils::ArgumentParser& arguments);

    typedef void (*CustomInit)();
    typedef void (*CustomDeinit)();

    /** Invokes @param init only once, calls @param deinit in destructor */
    static void customInit(CustomInit init, CustomDeinit deinit = nullptr);

    class InitGuard
    {
    public:
        InitGuard() { init(); }
        ~InitGuard() { deinit(); }

        InitGuard( const InitGuard& ) = delete;
        InitGuard( InitGuard&& ) = delete;
        InitGuard& operator=( const InitGuard& ) = delete;
        InitGuard& operator=( InitGuard&& ) = delete;
    };

private:
    SocketGlobals();
    ~SocketGlobals();
    void setDebugConfigurationTimer();

    enum class InitState { none, inintializing, done, deinitializing };

    static QnMutex s_mutex;
    static std::atomic<InitState> s_initState;
    static size_t s_counter;
    static SocketGlobals* s_instance;

private:
    // TODO: Initialization and deinitialization of this class is brocken by design (because of
    //     wrong dependencies). Should be fixed to separate singltones with strict dependencies:
    // 1. CommonSocketGlobals (AIO Service, DNS Resolver) - required for all system sockets.
    // 2. CloudSocketGlobals (cloud singletones) - required for cloud sockets.

    Config m_config;
    DebugConfiguration m_debugConfiguration;
    std::shared_ptr<QnLog::Logs> m_log;

    // Is unique_ptr because it should be initiated after m_aioService but removed after.
    std::unique_ptr<cloud::AddressResolver> m_addressResolver;

    aio::AIOService m_aioService;
    aio::Timer m_debugConfigurationTimer;

    // Is unique_ptr becaule it should be initiated before cloud classes but removed before.
    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;

    cloud::MediatorAddressPublisher m_addressPublisher;
    cloud::OutgoingTunnelPool m_outgoingTunnelPool;
    cloud::CloudConnectSettings m_cloudConnectSettings;
    cloud::tcp::ReverseConnectionPool m_tcpReversePool;

    QnMutex m_mutex;
    std::map<CustomInit, CustomDeinit> m_customInits;
};

class SocketGlobalsHolder
:
    public Singleton<SocketGlobalsHolder>
{
public:
    SocketGlobalsHolder()
    :
        m_socketGlobalsGuard(std::make_unique<SocketGlobals::InitGuard>())
    {
    }

    void reinitialize()
    {
        m_socketGlobalsGuard.reset();
        m_socketGlobalsGuard = std::make_unique<SocketGlobals::InitGuard>();
    }

private:
    std::unique_ptr<SocketGlobals::InitGuard> m_socketGlobalsGuard;
};

} // namespace network
} // namespace nx

#endif  //NX_NETWORK_SOCKET_GLOBAL_H
