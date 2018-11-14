#include "socket_global.h"

#include <map>

#include <nx/utils/std/cpp14.h>

#include "aio/aio_service.h"
#include "aio/pollset_factory.h"
#include "aio/timer.h"
#include "address_resolver.h"
#include "cloud/cloud_connect_controller.h"
#include "cloud/tunnel/outgoing_tunnel_pool.h" //< TODO: #ak Get rid of this dependency.
#include "socket_factory.h"
#include "ssl/ssl_static_data.h"

const std::chrono::seconds kDebugIniReloadInterval(10);

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

    void initialize()
    {
        m_aioService = std::make_unique<aio::AIOService>();
        m_aioService->initialize();
    }

    aio::AIOService& aioService()
    {
        return *m_aioService;
    }

private:
    std::unique_ptr<aio::AIOService> m_aioService;
};

enum class InitState { none, inintializing, done, deinitializing };

static QnMutex s_mutex;
static std::atomic<InitState> s_initState(InitState::none);
static size_t s_counter(0);
static SocketGlobals* s_instance = nullptr;

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

} // namespace

//-------------------------------------------------------------------------------------------------

struct SocketGlobalsImpl
{
    int m_initializationFlags = 0;
    Ini m_ini;
    /** map<string representation, regexp> */
    std::map<std::string, QRegExp> m_disabledHostPatterns;

    /**
     * Regular networking services. (AddressResolver should be split to cloud and non-cloud).
     */

#if defined(_WIN32)
    Win32SocketInitializer win32SocketInitializer;
#endif

    aio::PollSetFactory m_pollSetFactory;
    AioServiceGuard m_aioServiceGuard;
    std::unique_ptr<AddressResolver> m_addressResolver;
    std::unique_ptr<aio::Timer> m_debugIniReloadTimer;

    std::unique_ptr<cloud::CloudConnectController> cloudConnectController;

    QnMutex m_mutex;
    std::map<SocketGlobals::CustomInit, SocketGlobals::CustomDeinit> m_customInits;
};

//-------------------------------------------------------------------------------------------------

Ini::Ini():
    IniConfig("nx_network.ini")
{
    reload();
}

//-------------------------------------------------------------------------------------------------
// SocketGlobals

SocketGlobals::SocketGlobals(int initializationFlags):
    m_impl(std::make_unique<SocketGlobalsImpl>())
{
    m_impl->m_initializationFlags = initializationFlags;
    if (m_impl->m_initializationFlags & InitializationFlags::disableUdt)
        m_impl->m_pollSetFactory.disableUdt();

    reloadIni();
}

SocketGlobals::~SocketGlobals()
{
    deinitializeCloudConnectivity();

    m_impl->m_debugIniReloadTimer->pleaseStopSync();
    m_impl->m_addressResolver->pleaseStopSync();

    for (const auto& init: m_impl->m_customInits)
    {
        if (init.second)
            init.second();
    }
}

const Ini& SocketGlobals::ini()
{
    return s_instance->m_impl->m_ini;
}

aio::AIOService& SocketGlobals::aioService()
{
    return s_instance->m_impl->m_aioServiceGuard.aioService();
}

AddressResolver& SocketGlobals::addressResolver()
{
    return *s_instance->m_impl->m_addressResolver;
}

cloud::CloudConnectController& SocketGlobals::cloud()
{
    return *s_instance->m_impl->cloudConnectController;
}

int SocketGlobals::initializationFlags()
{
    return s_instance->m_impl->m_initializationFlags;
}

void SocketGlobals::init(
    int initializationFlags,
    const QString& customCloudHost)
{
    QnMutexLocker lock(&s_mutex);

    if (++s_counter == 1) //< First in.
    {
        s_initState = InitState::inintializing; //< Allow creating Pollable(s) in constructor.
        s_instance = new SocketGlobals(initializationFlags);

        s_instance->initializeNetworking();
        // TODO: #ak Disable cloud based on m_initializationFlags.
        s_instance->initializeCloudConnectivity(customCloudHost);

        s_initState = InitState::done;

        lock.unlock();
        s_instance->setDebugIniReloadTimer();
    }
}

void SocketGlobals::deinit()
{
    QnMutexLocker lock(&s_mutex);
    if (--s_counter == 0) //< Last out.
    {
        {
            QnMutexUnlocker unlock(&lock);
            delete s_instance;
        }
        s_initState = InitState::deinitializing; //< Allow creating Pollable(s) in destructor.
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

void SocketGlobals::printArgumentsHelp(std::ostream* outputStream)
{
    (*outputStream) <<
        "  --ip-version=, -ip               Ip version to use. 4 or 6" << std::endl <<
        "  --enforce-socket={socket type}   tcp, udt, cloud" << std::endl <<
        "  --enforce-ssl                    Use ssl for every connection" << std::endl;
    cloud::CloudConnectController::printArgumentsHelp(outputStream);
}

void SocketGlobals::applyArguments(const utils::ArgumentParser& arguments)
{
    if (const auto value = arguments.get("ip-version", "ip"))
        SocketFactory::setIpVersion(*value);

    if (const auto value = arguments.get("enforce-socket", "socket"))
        SocketFactory::enforceStreamSocketType(*value);

    if (arguments.get("enforce-ssl", "ssl"))
        SocketFactory::enforceSsl();

    cloud().applyArguments(arguments);
}

void SocketGlobals::customInit(CustomInit init, CustomDeinit deinit)
{
    QnMutexLocker lock(&s_instance->m_impl->m_mutex);
    if (s_instance->m_impl->m_customInits.emplace(init, deinit).second)
        init();
}

void SocketGlobals::blockHost(const std::string& regexpString)
{
    QnMutexLocker lock(&m_impl->m_mutex);

    m_impl->m_disabledHostPatterns.emplace(
        regexpString,
        QRegExp(regexpString.c_str(), Qt::CaseInsensitive, QRegExp::Wildcard));
}

void SocketGlobals::unblockHost(const std::string& regexpString)
{
    QnMutexLocker lock(&m_impl->m_mutex);

    m_impl->m_disabledHostPatterns.erase(regexpString);
}

bool SocketGlobals::isHostBlocked(const HostAddress& host) const
{
    // TODO: #ak Following check is not appropriate here.
    if (s_initState != InitState::done)
        return false;

    for (const auto& p: m_impl->m_disabledHostPatterns)
    {
        if (p.second.exactMatch(host.toString()))
            return true;
    }

    return false;
}

SocketGlobals& SocketGlobals::instance()
{
    return *s_instance;
}

void SocketGlobals::reloadIni()
{
    m_impl->m_ini.reload();

    const auto disabledHosts = QString::fromUtf8(m_impl->m_ini.disableHosts).split(',');
    for (const auto& regexpString: disabledHosts)
    {
        if (regexpString.isEmpty())
            continue;

        blockHost(regexpString.toStdString());
    }
}

void SocketGlobals::setDebugIniReloadTimer()
{
    m_impl->m_debugIniReloadTimer->start(
        kDebugIniReloadInterval,
        [this]()
        {
            reloadIni();
            setDebugIniReloadTimer();
        });
}

void SocketGlobals::initializeNetworking()
{
    m_impl->m_aioServiceGuard.initialize();

#ifdef ENABLE_SSL
    ssl::initOpenSSLGlobalLock();
#endif

    m_impl->m_addressResolver = std::make_unique<AddressResolver>();
    m_impl->m_debugIniReloadTimer = std::make_unique<aio::Timer>();
}

void SocketGlobals::initializeCloudConnectivity(const QString& customCloudHost)
{
    m_impl->cloudConnectController = std::make_unique<cloud::CloudConnectController>(
        customCloudHost,
        &m_impl->m_aioServiceGuard.aioService(),
        m_impl->m_addressResolver.get());
}

void SocketGlobals::deinitializeCloudConnectivity()
{
    m_impl->cloudConnectController.reset();
}

//-------------------------------------------------------------------------------------------------

SocketGlobalsHolder::SocketGlobalsHolder(int initializationFlags):
    m_initializationFlags(initializationFlags),
    m_socketGlobalsGuard(std::make_unique<SocketGlobals::InitGuard>(initializationFlags))
{
}

void SocketGlobalsHolder::reinitialize(bool initializePeerId)
{
    m_socketGlobalsGuard.reset();
    m_socketGlobalsGuard = std::make_unique<SocketGlobals::InitGuard>(m_initializationFlags);

    // TODO: #ak It is not clear why following call is in reinitialize,
    // but not in initial initialization. Remove it from here.
    if (initializePeerId)
        SocketGlobals::cloud().outgoingTunnelPool().assignOwnPeerId("re", QnUuid::createUuid());
}

} // namespace network
} // namespace nx
