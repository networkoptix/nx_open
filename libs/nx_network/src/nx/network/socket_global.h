// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include <nx/kit/ini_config.h>
#include <nx/utils/argument_parser.h>
#include <nx/utils/debug/allocation_analyzer.h>
#include <nx/utils/log/log.h>

#include "debug/object_counters.h"
#include "socket_common.h"

namespace nx {
namespace network {

namespace aio { class AIOService; }
class AddressResolver;

namespace http { class GlobalContext; }
namespace cloud { class CloudConnectController; }

struct SocketGlobalsImpl;

//-------------------------------------------------------------------------------------------------

/**
 * Contains objects that are required to use the network subsystem.
 * This object MUST be created before the first socket usage and destroyed after the last socket usage.
 * It can be created/destroyed with
 * <pre><code>
 * nx::network::SocketGlobalsHolder socketGlobals;
 * ...
 * </code></pre>
 * or
 * <pre><code>
 * nx::network::SocketGlobals::init();
 * ...
 * nx::network::SocketGlobals::deinit();
 * </code></pre>
 */
class NX_NETWORK_API SocketGlobals
{
public:
    static aio::AIOService& aioService();
    static AddressResolver& addressResolver();
    static http::GlobalContext& httpGlobalContext();
    static cloud::CloudConnectController& cloud();
    static int initializationFlags();

    /**
     * Allocates required resources. E.g., starts AIO threads.
     * Must be called before any socket use.
     * @param arguments Command-line arguments
     * The following command-line arguments are supported:
     * - aio-thread-pool-size Number of AIO threads to create. By default, 0 (the number of threads is auto-detected)
     * - cloud-host Cloud domain name to use
     */
    static void init(
        const ArgumentParser& arguments,
        int initializationFlags = 0);

    /**
     * Must be called before any socket use.
     */
    static void init(
        int initializationFlags = 0,
        const std::string& customCloudHost = std::string());

    static void deinit(); /**< Should be called when sockets are not needed any more. */
    static void verifyInitialization();
    static bool isInitialized();

    /**
     * Change the cloud host to the given value.
     * Warning! This function is not safe in any means. Do not use it directly in the production
     * code. For debug/support/developer purposes only.
     */
    static void switchCloudHost(const std::string& customCloudHost);

    static void printArgumentsHelp(std::ostream* outputStream);
    static void applyArguments(const ArgumentParser& arguments);

    bool isUdtEnabled() const;

    /**
     * Block sockets from connecting to a host that matches the regular expression given with
     * hostnameRegexp argument.
     */
    void blockHost(const std::string& hostnameRegexp);

    /**
     * Remove hostnameRegexp from the list of blocked hosts.
     */
    void unblockHost(const std::string& hostnameRegexp);

    /**
     * @return true if address has been blocked with SocketGlobals::blockHost.
     */
    bool isHostBlocked(const HostAddress& address) const;

    const debug::ObjectCounters& debugCounters() const;
    debug::ObjectCounters& debugCounters();

    nx::utils::debug::AllocationAnalyzer& allocationAnalyzer();

    static SocketGlobals& instance();

    class InitGuard
    {
    public:
        InitGuard(int initializationFlags = 0)
        {
            init(initializationFlags);
        }

        InitGuard(const ArgumentParser& arguments, int initializationFlags = 0)
        {
            init(arguments, initializationFlags);
        }

        ~InitGuard() { deinit(); }

        InitGuard(const InitGuard&) = delete;
        InitGuard(InitGuard&&) = delete;
        InitGuard& operator=(const InitGuard&) = delete;
        InitGuard& operator=(InitGuard&&) = delete;
    };

private:
    std::unique_ptr<SocketGlobalsImpl> m_implGuard;
    SocketGlobalsImpl* m_impl = nullptr;
    debug::ObjectCounters m_debugCounters;
    nx::utils::debug::AllocationAnalyzer m_allocationAnalyzer;

    /**
     * @param initializationFlags Bitset of nx::network::InitializationFlags.
     */
    SocketGlobals(int initializationFlags);
    ~SocketGlobals();

    SocketGlobals(const SocketGlobals&) = delete;
    SocketGlobals& operator=(const SocketGlobals&) = delete;

    void reloadIni();
    void setDebugIniReloadTimer();

    void initializeNetworking(const ArgumentParser& arguments);

    void initializeCloudConnectivity(const std::string& customCloudHost);
    void deinitializeCloudConnectivity();
};

//-------------------------------------------------------------------------------------------------

/**
 * Invokes SocketGlobals::init() when constructed and SocketGlobals::deinit() just before destruction.
 */
class NX_NETWORK_API SocketGlobalsHolder
{
public:
    /**
     * Invokes SocketGlobalsHolder::initialize(false).
     */
    SocketGlobalsHolder(int initializationFlags = 0);
    SocketGlobalsHolder(const ArgumentParser& arguments, int initializationFlags = 0);
    ~SocketGlobalsHolder();

    void initialize(bool initializePeerId = true);
    void uninitialize();
    void reinitialize(bool initializePeerId = true);

    static SocketGlobalsHolder* instance();

private:
    const nx::ArgumentParser m_args;
    const int m_initializationFlags;
    std::unique_ptr<SocketGlobals::InitGuard> m_socketGlobalsGuard;
};

} // namespace network
} // namespace nx
