#pragma once

#include <memory>

#include <nx/kit/ini_config.h>

#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log.h>
#include <nx/utils/singleton.h>

#include "socket_common.h"

namespace nx {

namespace hpm { namespace api { class MediatorConnector; } }

namespace network {

namespace aio { class AIOService; }

namespace cloud {

class AddressResolver;
class MediatorAddressPublisher;
class OutgoingTunnelPool;
class CloudConnectSettings;
namespace tcp { class ReverseConnectionPool; }

} // namespace cloud

struct SocketGlobalsImpl;

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

    static Ini& ini();
    static DebugIni& debugIni();
    static aio::AIOService& aioService();
    static cloud::AddressResolver& addressResolver();
    static cloud::MediatorAddressPublisher& addressPublisher();
    static hpm::api::MediatorConnector& mediatorConnector();
    static cloud::OutgoingTunnelPool& outgoingTunnelPool();
    static cloud::CloudConnectSettings& cloudConnectSettings();
    static cloud::tcp::ReverseConnectionPool& tcpReversePool();
    static int initializationFlags();

    static void init(int initializationFlags = 0); /**< Should be called before any socket use */
    static void deinit(); /**< Should be called when sockets are not needed any more */
    static void verifyInitialization();
    static bool isInitialized();

    static void printArgumentsHelp(std::ostream* outputStream);
    static void applyArguments(const utils::ArgumentParser& arguments);

    typedef void (*CustomInit)();
    typedef void (*CustomDeinit)();

    /** Invokes @param init only once, calls @param deinit in destructor. */
    static void customInit(CustomInit init, CustomDeinit deinit = nullptr);

    class InitGuard
    {
    public:
        InitGuard(int initializationFlags = 0) { init(initializationFlags); }
        ~InitGuard() { deinit(); }

        InitGuard(const InitGuard&) = delete;
        InitGuard(InitGuard&&) = delete;
        InitGuard& operator=(const InitGuard&) = delete;
        InitGuard& operator=(InitGuard&&) = delete;
    };

private:
    /**
     * @param initializationFlags Bitset of nx::network::InitializationFlags.
     */
    SocketGlobals(int initializationFlags);
    ~SocketGlobals();

    SocketGlobals(const SocketGlobals&) = delete;
    SocketGlobals& operator=(const SocketGlobals&) = delete;

    void setDebugIniReloadTimer();

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

    std::unique_ptr<SocketGlobalsImpl> m_impl;

    void initializeCloudConnectivity();
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API SocketGlobalsHolder:
    public Singleton<SocketGlobalsHolder>
{
public:
    SocketGlobalsHolder(int initializationFlags = 0);

    void reinitialize(bool initializePeerId = true);

private:
    const int m_initializationFlags;
    std::unique_ptr<SocketGlobals::InitGuard> m_socketGlobalsGuard;
};

} // namespace network
} // namespace nx
