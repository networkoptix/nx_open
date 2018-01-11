#pragma once

#include <memory>

#include <nx/kit/ini_config.h>

#include <nx/utils/argument_parser.h>
#include <nx/utils/log/log.h>
#include <nx/utils/singleton.h>

#include "socket_common.h"

namespace nx {
namespace network {

namespace aio { class AIOService; }
class AddressResolver;

namespace cloud { class CloudConnectController; }

struct SocketGlobalsImpl;

class NX_NETWORK_API SocketGlobals
{
public:
    using CustomInit = void(*)();
    using CustomDeinit = void(*)();

    struct DebugIni: nx::kit::IniConfig
    {
        DebugIni(): IniConfig("nx_network_debug.ini") { reload(); }

        NX_INI_FLAG(0, httpClientTraffic, "Trace HTTP traffic for nx::network::http::AsyncHttpClient");
        NX_INI_STRING("", cloudHost, "Overridden Cloud Host");
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
    static AddressResolver& addressResolver();
    static cloud::CloudConnectController& cloud();
    static int initializationFlags();

    static void init(int initializationFlags = 0); /**< Should be called before any socket use. */
    static void deinit(); /**< Should be called when sockets are not needed any more. */
    static void verifyInitialization();
    static bool isInitialized();

    static void printArgumentsHelp(std::ostream* outputStream);
    static void applyArguments(const utils::ArgumentParser& arguments);

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
    std::unique_ptr<SocketGlobalsImpl> m_impl;

    /**
     * @param initializationFlags Bitset of nx::network::InitializationFlags.
     */
    SocketGlobals(int initializationFlags);
    ~SocketGlobals();

    SocketGlobals(const SocketGlobals&) = delete;
    SocketGlobals& operator=(const SocketGlobals&) = delete;

    void setDebugIniReloadTimer();

    void initializeNetworking();

    void initializeCloudConnectivity();
    void deinitializeCloudConnectivity();
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
