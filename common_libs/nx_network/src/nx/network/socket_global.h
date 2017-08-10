#pragma once

#include <memory>

#include <nx/kit/ini_config.h>

#include <nx/utils/log/log.h>
#include <nx/utils/singleton.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/std/cpp14.h>

#include "aio/aio_service.h"
#include "aio/pollset_factory.h"

#include "cloud/address_resolver.h"
#include "cloud/cloud_connect_settings.h"
#include "cloud/mediator_address_publisher.h"
#include "cloud/mediator_connector.h"
#include "cloud/tunnel/outgoing_tunnel_pool.h"
#include "cloud/tunnel/tcp/reverse_connection_pool.h"
#include "socket_common.h"

namespace nx {
namespace network {

class NX_NETWORK_API SocketGlobals
{
public:
    struct DebugIni: nx::kit::IniConfig
    {
        DebugIni(): IniConfig("nx_network_debug.ini") { reload(); }

        NX_INI_FLAG(0, httpClientTraffic, "Trace HTTP traffic for nx_http::AsyncHttpClient");

        // TODO: Should be moved to a different flag config, because module finders live in common.
        // This flag resides here just because there are no other flag configs for logging.
        NX_INI_INT(0, multicastModuleFinderTimeout, "Use timeout instead of poll in QnMMF");
    };

    struct Ini: nx::kit::IniConfig
    {
        Ini(): IniConfig("nx_network.ini") { reload(); }

        NX_INI_FLAG(0, disableCloudSockets, "Use plain TCP sockets instead of Cloud sockets");
        NX_INI_STRING("", disableHosts, "Comma-separated list of forbidden IPs and domains");

        bool isHostDisabled(const HostAddress& address) const;
    };

    typedef cloud::MediatorAddressPublisher AddressPublisher;
    typedef hpm::api::MediatorConnector MediatorConnector;
    typedef cloud::OutgoingTunnelPool OutgoingTunnelPool;
    typedef cloud::CloudConnectSettings CloudSettings;
    typedef cloud::tcp::ReverseConnectionPool TcpReversePool;

    static Ini& ini() { return s_instance->m_ini; }
    static DebugIni& debugIni() { return s_instance->m_debugIni; }
    static aio::AIOService& aioService() { return s_instance->m_aioServiceGuard.aioService(); }
    static cloud::AddressResolver& addressResolver() { return *s_instance->m_addressResolver; }
    static AddressPublisher& addressPublisher() { return *s_instance->m_addressPublisher; }
    static MediatorConnector& mediatorConnector() { return *s_instance->m_mediatorConnector; }
    static OutgoingTunnelPool& outgoingTunnelPool() { return *s_instance->m_outgoingTunnelPool; }
    static CloudSettings& cloudConnectSettings() { return s_instance->m_cloudConnectSettings; }
    static TcpReversePool& tcpReversePool() { return *s_instance->m_tcpReversePool; }
    static int initializationFlags() { return s_instance->m_initializationFlags; }

    static void init(int initializationFlags = 0); /**< Should be called before any socket use */
    static void deinit(); /**< Should be called when sockets are not needed any more */
    static void verifyInitialization();
    static bool isInitialized();

    static void applyArguments(const utils::ArgumentParser& arguments);

    typedef void (*CustomInit)();
    typedef void (*CustomDeinit)();

    /** Invokes @param init only once, calls @param deinit in destructor */
    static void customInit(CustomInit init, CustomDeinit deinit = nullptr);

    class InitGuard
    {
    public:
        InitGuard(int initializationFlags = 0) { init(initializationFlags); }
        ~InitGuard() { deinit(); }

        InitGuard( const InitGuard& ) = delete;
        InitGuard( InitGuard&& ) = delete;
        InitGuard& operator=( const InitGuard& ) = delete;
        InitGuard& operator=( InitGuard&& ) = delete;
    };

private:
    /**
     * @param initializationFlags Bitset of nx::network::InitializationFlags.
     */
    SocketGlobals(int initializationFlags);
    ~SocketGlobals();
    void setDebugIniReloadTimer();

    enum class InitState { none, inintializing, done, deinitializing };

    static QnMutex s_mutex;
    static std::atomic<InitState> s_initState;
    static size_t s_counter;
    static SocketGlobals* s_instance;

private:
    class AioServiceGuard
    {
    public:
        AioServiceGuard();
        ~AioServiceGuard();

        void initialize();

        aio::AIOService& aioService();

    private:
        std::unique_ptr<aio::AIOService> m_aioService;
    };

    // TODO: Initialization and deinitialization of this class is brocken by design (because of
    //     wrong dependencies). Should be fixed to separate singltones with strict dependencies:
    // 1. CommonSocketGlobals (AIO Service, DNS Resolver) - required for all system sockets.
    // 2. CloudSocketGlobals (cloud singletones) - required for cloud sockets.

    const int m_initializationFlags;
    Ini m_ini;
    DebugIni m_debugIni;

    // Is unique_ptr because it should be initiated after m_aioService but removed after.
    std::unique_ptr<cloud::AddressResolver> m_addressResolver;

    aio::PollSetFactory m_pollSetFactory;
    AioServiceGuard m_aioServiceGuard;
    std::unique_ptr<aio::Timer> m_debugIniReloadTimer;

    // Is unique_ptr becaule it should be initiated before cloud classes but removed before.
    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;

    std::unique_ptr<cloud::MediatorAddressPublisher> m_addressPublisher;
    std::unique_ptr<cloud::OutgoingTunnelPool> m_outgoingTunnelPool;
    cloud::CloudConnectSettings m_cloudConnectSettings;
    std::unique_ptr<cloud::tcp::ReverseConnectionPool> m_tcpReversePool;

    QnMutex m_mutex;
    std::map<CustomInit, CustomDeinit> m_customInits;

    void initializeCloudConnectivity();
};

class SocketGlobalsHolder:
    public Singleton<SocketGlobalsHolder>
{
public:
    SocketGlobalsHolder(int initializationFlags = 0):
        m_initializationFlags(initializationFlags),
        m_socketGlobalsGuard(std::make_unique<SocketGlobals::InitGuard>(initializationFlags))
    {
    }

    void reinitialize(bool initializePeerId = true)
    {
        m_socketGlobalsGuard.reset();
        m_socketGlobalsGuard = std::make_unique<SocketGlobals::InitGuard>(m_initializationFlags);

        if (initializePeerId)
            SocketGlobals::outgoingTunnelPool().assignOwnPeerId("re", QnUuid::createUuid());
    }

private:
    const int m_initializationFlags;
    std::unique_ptr<SocketGlobals::InitGuard> m_socketGlobalsGuard;
};

} // namespace network
} // namespace nx
