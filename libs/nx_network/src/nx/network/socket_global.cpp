#include "socket_global.h"

#include <map>

#include <udt/udt.h>

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
    Ini ini;
    /** map<string representation, regexp> */
    std::map<std::string, QRegExp> disabledHostPatterns;

    /**
     * Regular networking services. (AddressResolver should be split to cloud and non-cloud).
     */

#if defined(_WIN32)
    Win32SocketInitializer win32SocketInitializer;
#endif
    std::unique_ptr<UdtInitializer> udtInitializer;

    aio::PollSetFactory pollSetFactory;
    AioServiceGuard aioServiceGuard;
    std::unique_ptr<AddressResolver> addressResolver;
    std::unique_ptr<aio::Timer> debugIniReloadTimer;

    std::unique_ptr<cloud::CloudConnectController> cloudConnectController;

    QnMutex mutex;
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
}

const Ini& SocketGlobals::ini()
{
    return s_instance->m_impl->ini;
}

aio::AIOService& SocketGlobals::aioService()
{
    return s_instance->m_impl->aioServiceGuard.aioService();
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

void SocketGlobals::init(
    int initializationFlags,
    const QString& customCloudHost)
{
    QnMutexLocker lock(&s_mutex);

    if (++s_counter == 1) //< First in.
    {
        s_initState = InitState::inintializing; //< Allow creating Pollables in constructor.
        s_instance = new SocketGlobals(initializationFlags);

        s_instance->initializeNetworking();
        // TODO: #ak Disable cloud based on initializationFlags.
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

bool SocketGlobals::isUdtEnabled() const
{
    return (m_impl->initializationFlags & InitializationFlags::disableUdt) == 0;
}

void SocketGlobals::blockHost(const std::string& regexpString)
{
    QnMutexLocker lock(&m_impl->mutex);

    m_impl->disabledHostPatterns.emplace(
        regexpString,
        QRegExp(regexpString.c_str(), Qt::CaseInsensitive, QRegExp::Wildcard));
}

void SocketGlobals::unblockHost(const std::string& regexpString)
{
    QnMutexLocker lock(&m_impl->mutex);

    m_impl->disabledHostPatterns.erase(regexpString);
}

bool SocketGlobals::isHostBlocked(const HostAddress& host) const
{
    // TODO: #ak Following check is not appropriate here.
    if (s_initState != InitState::done)
        return false;

    for (const auto& p: m_impl->disabledHostPatterns)
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
    m_impl->ini.reload();

    const auto disabledHosts = QString::fromUtf8(m_impl->ini.disableHosts).split(',');
    for (const auto& regexpString: disabledHosts)
    {
        if (regexpString.isEmpty())
            continue;

        blockHost(regexpString.toStdString());
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

void SocketGlobals::initializeNetworking()
{
    if (isUdtEnabled())
        m_impl->udtInitializer = std::make_unique<UdtInitializer>();

    m_impl->aioServiceGuard.initialize();

#ifdef ENABLE_SSL
    ssl::initOpenSSLGlobalLock();
#endif

    m_impl->addressResolver = std::make_unique<AddressResolver>();
    m_impl->debugIniReloadTimer = std::make_unique<aio::Timer>();
}

void SocketGlobals::initializeCloudConnectivity(const QString& customCloudHost)
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
