// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "socket_global.h"

#include <map>
#include <optional>

#include <QtCore/QRegularExpression>

#include <udt/udt.h>

#include <nx/utils/debug.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/string.h>

#include "aio/aio_service.h"
#include "aio/pollset_factory.h"
#include "aio/timer.h"
#include "address_resolver.h"
#include "cloud/cloud_connect_controller.h"
#include "cloud/tunnel/outgoing_tunnel_pool.h" //< TODO: #akolesnikov Get rid of this dependency.
#include "http/global_context.h"
#include "nx_network_ini.h"
#include "socket_factory.h"
#include "ssl/context.h"

static constexpr std::chrono::seconds kDebugIniReloadInterval{10};

template<>
nx::network::SocketGlobalsHolder* Singleton<nx::network::SocketGlobalsHolder>::s_instance =
    nullptr;

namespace nx {
namespace network {

namespace {

class AioServiceGuard
{
public:
    ~AioServiceGuard()
    {
        if (m_aioService)
            m_aioService->pleaseStopSync();
    }

    void initialize(unsigned int aioThreadPoolSize)
    {
        m_aioService = std::make_unique<aio::AIOService>();
        m_aioService->initialize(aioThreadPoolSize);
    }

    aio::AIOService& aioService()
    {
        return *m_aioService;
    }

private:
    std::unique_ptr<aio::AIOService> m_aioService;
};

enum class InitState { none, inintializing, done, deinitializing };

static std::atomic<InitState> s_initState(InitState::none);
static size_t s_counter(0);
static SocketGlobals* s_instance = nullptr;
static SocketGlobalsHolder* s_holderInstance = nullptr;

//-------------------------------------------------------------------------------------------------

#if defined(_WIN32)
class Win32SocketInitializer
{
public:
    Win32SocketInitializer()
    {
        WSADATA wsaData;
        WORD versionRequested = MAKEWORD(2, 0);
        WSAStartup(versionRequested, &wsaData);
    }

    ~Win32SocketInitializer()
    {
        WSACleanup();
    }
};
#endif

class UdtInitializer
{
public:
    UdtInitializer()
    {
        UDT::startup();
    }

    ~UdtInitializer()
    {
        UDT::cleanup();
    }
};

} // namespace

//-------------------------------------------------------------------------------------------------

struct SocketGlobalsImpl
{
    int initializationFlags = 0;

    /** map<string representation, regexp> */
    std::map<std::string, QRegularExpression> hostDisabledProgrammatically;
    std::map<std::string, QRegularExpression> hostDisabledViaIni;

    /**
     * Regular networking services. (AddressResolver should be split to cloud and non-cloud).
     */

#if defined(_WIN32)
    Win32SocketInitializer win32SocketInitializer;
#endif
    std::unique_ptr<UdtInitializer> udtInitializer;

    std::optional<nx::utils::debug::OnAboutToBlockHandler> onAboutToBlockHandler;

    aio::PollSetFactory pollSetFactory;
    std::unique_ptr<AddressResolver> addressResolver;
    AioServiceGuard aioServiceGuard;
    std::unique_ptr<http::GlobalContext> httpGlobalContext;
    std::unique_ptr<aio::Timer> debugIniReloadTimer;

    std::unique_ptr<cloud::CloudConnectController> cloudConnectController;

    nx::Mutex mutex;
};

//-------------------------------------------------------------------------------------------------
// SocketGlobals

SocketGlobals::SocketGlobals(int initializationFlags):
    m_implGuard(std::make_unique<SocketGlobalsImpl>()),
    m_impl(m_implGuard.get()),
    m_allocationAnalyzer(/*isEnabled*/ ini().traceIoObjectsLifetime)
{
    m_impl->initializationFlags = initializationFlags;
    if (m_impl->initializationFlags & InitializationFlags::disableUdt)
        m_impl->pollSetFactory.disableUdt();

    reloadIni();
}

SocketGlobals::~SocketGlobals()
{
    deinitializeCloudConnectivity();

    m_impl->debugIniReloadTimer->pleaseStopSync();
    m_impl->addressResolver->pleaseStopSync();

    if (m_impl->onAboutToBlockHandler)
    {
        nx::utils::debug::setOnAboutToBlockHandler(
            *std::exchange(m_impl->onAboutToBlockHandler, std::nullopt));
    }

    m_implGuard.reset();
    // Keeping m_impl pointer accessible while SocketGlobalsImpl is being destroyed
    // so that its members are still able to access each other until they are destroyed.
    // TODO: #akolesnikov Decouple life-time of the holder of everything that currently resides in
    // SocketGlobalsImpl and the members of SocketGlobalsImpl so that no one accesses
    // SocketGlobalsImpl when the control is in its destructor.
    m_impl = nullptr;

    const std::pair<const char*, int> counters[] = {
        {"HTTP Client Connections", m_debugCounters.httpClientConnectionCount},
        {"HTTP Server Connections", m_debugCounters.httpServerConnectionCount},
        {"TCP Sockets", m_debugCounters.tcpSocketCount},
        {"UDP Sockets", m_debugCounters.udpSocketCount}
    };

    for (const auto& [key, value]: counters)
    {
        NX_ASSERT(value == 0, "There are %1 %2 left, allocation report%3", key, value,
            [this]
            {
                if (const auto r = m_allocationAnalyzer.generateReport())
                    return NX_FMT(":\n%1", *r);

                return NX_FMT(" is disabled, enable traceIoObjectsLifetime in nx_network.ini");
            }());
    }
}

aio::AIOService& SocketGlobals::aioService()
{
    return s_instance->m_impl->aioServiceGuard.aioService();
}

http::GlobalContext& SocketGlobals::httpGlobalContext()
{
    return *s_instance->m_impl->httpGlobalContext;
}

AddressResolver& SocketGlobals::addressResolver()
{
    return *s_instance->m_impl->addressResolver;
}

cloud::CloudConnectController& SocketGlobals::cloud()
{
    return *s_instance->m_impl->cloudConnectController;
}

int SocketGlobals::initializationFlags()
{
    return s_instance->m_impl->initializationFlags;
}

static nx::Mutex& socketGlobalsMutexInstance()
{
    static nx::Mutex mtx;
    return mtx;
}

void SocketGlobals::init(
    const ArgumentParser& arguments,
    int initializationFlags)
{
    NX_MUTEX_LOCKER lock(&socketGlobalsMutexInstance());

    if (++s_counter == 1) //< First in.
    {
        s_initState = InitState::inintializing; //< Allow creating Pollables in constructor.
        s_instance = new SocketGlobals(initializationFlags);

        s_instance->initializeNetworking(arguments);

        // TODO: #akolesnikov Disable cloud based on initializationFlags.
        std::string customCloudHost;
        arguments.read("cloud-host", &customCloudHost);
        s_instance->initializeCloudConnectivity(customCloudHost);

        s_initState = InitState::done;

        lock.unlock();
        s_instance->setDebugIniReloadTimer();

        applyArguments(arguments);
    }
}

void SocketGlobals::init(
    int initializationFlags,
    const std::string& customCloudHost)
{
    ArgumentParser args;
    if (!customCloudHost.empty())
        args.parse({nx::format("--cloud-host=%1").args(customCloudHost)});

    init(args, initializationFlags);
}

void SocketGlobals::deinit()
{
    NX_MUTEX_LOCKER lock(&socketGlobalsMutexInstance());
    if (--s_counter == 0) //< Last out.
    {
        {
            nx::Unlocker<nx::Mutex> unlock(&lock);
            delete s_instance;
        }
        s_initState = InitState::deinitializing; //< Allow creating Pollables in destructor.
        s_instance = nullptr;
        s_initState = InitState::none;
    }
}

void SocketGlobals::verifyInitialization()
{
    NX_CRITICAL(
        s_initState != InitState::none,
        "SocketGlobals::InitGuard must be initialized before using Sockets");
}

bool SocketGlobals::isInitialized()
{
    return s_instance != nullptr;
}

void SocketGlobals::switchCloudHost(const std::string& customCloudHost)
{
    const std::string ownPeerId = cloud().outgoingTunnelPool().ownPeerId();
    s_instance->deinitializeCloudConnectivity();
    s_instance->initializeCloudConnectivity(customCloudHost);
    cloud().outgoingTunnelPool().setOwnPeerId(ownPeerId);
}

void SocketGlobals::printArgumentsHelp(std::ostream* outputStream)
{
    (*outputStream)
        << "  --ip-version=, -ip               Ip version to use. 4 or 6" << std::endl
        << "  --enforce-socket={socket type}   tcp, udt, cloud" << std::endl;
    cloud::CloudConnectController::printArgumentsHelp(outputStream);
}

void SocketGlobals::applyArguments(const ArgumentParser& arguments)
{
    if (const auto value = arguments.get("ip-version", "ip"))
        SocketFactory::setIpVersion(value->toStdString());

    if (const auto value = arguments.get("enforce-socket", "socket"))
        SocketFactory::enforceStreamSocketType(value->toStdString());

    cloud().applyArguments(arguments);
}

bool SocketGlobals::isUdtEnabled() const
{
    return (m_impl->initializationFlags & InitializationFlags::disableUdt) == 0;
}

void SocketGlobals::blockHost(const std::string& regexpString)
{
    NX_MUTEX_LOCKER lock(&m_impl->mutex);

    const auto wildcard =
        QRegularExpression::wildcardToRegularExpression(QString::fromStdString(regexpString));

    m_impl->hostDisabledProgrammatically.emplace(
        regexpString,
        QRegularExpression(wildcard, QRegularExpression::CaseInsensitiveOption));
}

void SocketGlobals::unblockHost(const std::string& regexpString)
{
    NX_MUTEX_LOCKER lock(&m_impl->mutex);

    m_impl->hostDisabledProgrammatically.erase(regexpString);
}

static bool matchHost(
    const std::map<std::string, QRegularExpression>& patterns,
    const HostAddress& host)
{
    for (const auto& p: patterns)
    {
        if (p.second.match(QString::fromStdString(host.toString())).hasMatch())
            return true;
    }

    return false;
}

bool SocketGlobals::isHostBlocked(const HostAddress& host) const
{
    // TODO: #akolesnikov Following check is not appropriate here.
    if (s_initState != InitState::done)
        return false;

    NX_MUTEX_LOCKER lock(&m_impl->mutex);

    return matchHost(m_impl->hostDisabledProgrammatically, host)
        || matchHost(m_impl->hostDisabledViaIni, host);
}

SocketGlobals& SocketGlobals::instance()
{
    return *s_instance;
}

const debug::ObjectCounters& SocketGlobals::debugCounters() const
{
    return m_debugCounters;
}

debug::ObjectCounters& SocketGlobals::debugCounters()
{
    return m_debugCounters;
}

nx::utils::debug::AllocationAnalyzer& SocketGlobals::allocationAnalyzer()
{
    return m_allocationAnalyzer;
}

void SocketGlobals::reloadIni()
{
    ini().reload();

    NX_MUTEX_LOCKER lock(&m_impl->mutex);

    m_impl->hostDisabledViaIni.clear();

    std::vector<std::string_view> disabledHosts;
    nx::utils::split(
        std::string_view(ini().disableHosts), ',',
        std::back_inserter(disabledHosts));
    for (const auto& regexpString: disabledHosts)
    {
        if (regexpString.empty())
            continue;

        const auto wildcard =
            QRegularExpression::wildcardToRegularExpression(
                QString::fromUtf8(regexpString.data(), regexpString.size()));

        m_impl->hostDisabledViaIni.emplace(
            regexpString,
            QRegularExpression(wildcard, QRegularExpression::CaseInsensitiveOption));
    }
}

void SocketGlobals::setDebugIniReloadTimer()
{
    m_impl->debugIniReloadTimer->start(
        kDebugIniReloadInterval,
        [this]()
        {
            reloadIni();
            setDebugIniReloadTimer();
        });
}

void SocketGlobals::initializeNetworking(const ArgumentParser& arguments)
{
    if (isUdtEnabled())
        m_impl->udtInitializer = std::make_unique<UdtInitializer>();

    int aioThreadCount = 0;
    arguments.read("aio-thread-count", &aioThreadCount);
    m_impl->aioServiceGuard.initialize(aioThreadCount);

    m_impl->onAboutToBlockHandler = nx::utils::debug::setOnAboutToBlockHandler(
        []() { NX_ASSERT(!aioService().isInAnyAioThread()); });

    m_impl->addressResolver = std::make_unique<AddressResolver>();
    m_impl->httpGlobalContext = std::make_unique<http::GlobalContext>();
    m_impl->debugIniReloadTimer = std::make_unique<aio::Timer>();
}

void SocketGlobals::initializeCloudConnectivity(const std::string& customCloudHost)
{
    m_impl->cloudConnectController = std::make_unique<cloud::CloudConnectController>(
        customCloudHost,
        &m_impl->aioServiceGuard.aioService(),
        m_impl->addressResolver.get());
}

void SocketGlobals::deinitializeCloudConnectivity()
{
    m_impl->cloudConnectController.reset();
}

//-------------------------------------------------------------------------------------------------

SocketGlobalsHolder::SocketGlobalsHolder(int initializationFlags):
    SocketGlobalsHolder({}, initializationFlags)
{
}

SocketGlobalsHolder::SocketGlobalsHolder(
    const ArgumentParser& arguments,
    int initializationFlags)
    :
    m_args(arguments),
    m_initializationFlags(initializationFlags)
{
    if (s_holderInstance)
        NX_ERROR(this, "Singleton is created more than once.");
    else
        s_holderInstance = this;

    initialize(/*initializePeerId*/ false);
}

SocketGlobalsHolder::~SocketGlobalsHolder()
{
    if (s_holderInstance == this)
        s_holderInstance = nullptr;
}

void SocketGlobalsHolder::initialize(bool initializePeerId)
{
    m_socketGlobalsGuard = std::make_unique<SocketGlobals::InitGuard>(m_args, m_initializationFlags);

    // TODO: #akolesnikov It is not clear why following call is in reinitialize,
    // but not in initial initialization. Remove it from here.
    if (initializePeerId)
        SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId("re", nx::Uuid::createUuid());
}

void SocketGlobalsHolder::uninitialize()
{
    m_socketGlobalsGuard.reset();
}

void SocketGlobalsHolder::reinitialize(bool initializePeerId)
{
    uninitialize();
    initialize(initializePeerId);
}

SocketGlobalsHolder* SocketGlobalsHolder::instance()
{
    return s_holderInstance;
}

} // namespace network
} // namespace nx
