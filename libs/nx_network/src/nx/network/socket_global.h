#pragma once

#include <atomic>
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

struct NX_NETWORK_API Ini:
    nx::kit::IniConfig
{
    Ini();

    NX_INI_FLAG(0, httpClientTraffic, "Trace HTTP traffic for nx::network::http::AsyncHttpClient");
    NX_INI_STRING("", disableHosts, "Comma-separated list of forbidden IPs and domains");
};

struct DebugCounters
{
    std::atomic<int> tcpSocketCount{0};
    std::atomic<int> stunConnectionCount{0};
    std::atomic<int> httpServerConnectionCount{0};
};

class NX_NETWORK_API SocketGlobals
{
public:
    static const Ini& ini();
    static aio::AIOService& aioService();
    static AddressResolver& addressResolver();
    static cloud::CloudConnectController& cloud();
    static int initializationFlags();

    static void init(
        int initializationFlags = 0,
        const QString& customCloudHost = QString()); /**< Should be called before any socket use. */
    static void deinit(); /**< Should be called when sockets are not needed any more. */
    static void verifyInitialization();
    static bool isInitialized();

    static void printArgumentsHelp(std::ostream* outputStream);
    static void applyArguments(const utils::ArgumentParser& arguments);

    bool isUdtEnabled() const;

    void blockHost(const std::string& hostname);
    void unblockHost(const std::string& hostname);
    bool isHostBlocked(const HostAddress& address) const;

    const DebugCounters& debugCounters() const;
    DebugCounters& debugCounters();

    static SocketGlobals& instance();

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
    DebugCounters m_debugCounters;

    /**
     * @param initializationFlags Bitset of nx::network::InitializationFlags.
     */
    SocketGlobals(int initializationFlags);
    ~SocketGlobals();

    SocketGlobals(const SocketGlobals&) = delete;
    SocketGlobals& operator=(const SocketGlobals&) = delete;

    void reloadIni();
    void setDebugIniReloadTimer();

    void initializeNetworking();

    void initializeCloudConnectivity(const QString& customCloudHost);
    void deinitializeCloudConnectivity();
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API SocketGlobalsHolder:
    public Singleton<SocketGlobalsHolder>
{
public:
    /**
     * Invokes SocketGlobalsHolder::initialize(false).
     */
    SocketGlobalsHolder(int initializationFlags = 0);

    void initialize(bool initializePeerId = true);
    void uninitialize();
    void reinitialize(bool initializePeerId = true);

private:
    const int m_initializationFlags;
    std::unique_ptr<SocketGlobals::InitGuard> m_socketGlobalsGuard;
};

} // namespace network
} // namespace nx
