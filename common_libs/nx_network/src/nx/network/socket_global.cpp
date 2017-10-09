#include "socket_global.h"

#include <nx/utils/thread/barrier_handler.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>

#include "aio/pollset_factory.h"
#include "aio/aio_service.h"
#include "cloud/address_resolver.h"
#include "cloud/tunnel/connector_factory.h"
#include "cloud/cloud_connect_settings.h"
#include "cloud/mediator_address_publisher.h"
#include "cloud/mediator_connector.h"
#include "cloud/tunnel/outgoing_tunnel_pool.h"
#include "cloud/tunnel/tcp/reverse_connection_pool.h"
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
    }

    aio::AIOService& aioService()
    {
        return *m_aioService;
    }

private:
    std::unique_ptr<aio::AIOService> m_aioService;
};

} // namespace

//-------------------------------------------------------------------------------------------------

struct SocketGlobalsImpl
{
    int m_initializationFlags = 0;
    SocketGlobals::Ini m_ini;
    SocketGlobals::DebugIni m_debugIni;

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
    std::map<SocketGlobals::CustomInit, SocketGlobals::CustomDeinit> m_customInits;
};

//-------------------------------------------------------------------------------------------------

bool SocketGlobals::Ini::isHostDisabled(const HostAddress& host) const
{
    if (SocketGlobals::s_initState != InitState::done)
        return false;

    // Here 'static const' is an optimization as reload is called only on start.
    static const auto disabledHostPatterns =
        [this]()
        {
            std::list<QRegExp> regExps;
            for (const auto& s: QString::fromUtf8(disableHosts).split(QChar(',')))
            {
                if (!s.isEmpty())
                    regExps.push_back(QRegExp(s, Qt::CaseInsensitive, QRegExp::Wildcard));
            }

            return regExps;
        }();

    for (const auto& p: disabledHostPatterns)
    {
        if (p.exactMatch(host.toString()))
            return true;
    }

    return false;
}

//-------------------------------------------------------------------------------------------------
// SocketGlobals

SocketGlobals::SocketGlobals(int initializationFlags):
    m_impl(std::make_unique<SocketGlobalsImpl>())
{
    m_impl->m_initializationFlags = initializationFlags;
    if (m_impl->m_initializationFlags & InitializationFlags::disableUdt)
        m_impl->m_pollSetFactory.disableUdt();

    m_impl->m_aioServiceGuard.initialize();

#ifdef ENABLE_SSL
    ssl::initOpenSSLGlobalLock();
#endif
}

SocketGlobals::~SocketGlobals()
{
    // NOTE: should be moved to QnStoppableAsync::pleaseStop

    nx::utils::promise<void> cloudServicesStoppedPromise;
    {
        utils::BarrierHandler barrier([&](){ cloudServicesStoppedPromise.set_value(); });
        m_impl->m_debugIniReloadTimer->pleaseStop(barrier.fork());
        m_impl->m_addressResolver->pleaseStop(barrier.fork());
        m_impl->m_addressPublisher->pleaseStop(barrier.fork());
        m_impl->m_outgoingTunnelPool->pleaseStop(barrier.fork());
        m_impl->m_tcpReversePool->pleaseStop(barrier.fork());
    }

    cloudServicesStoppedPromise.get_future().wait();

    for (const auto& init: m_impl->m_customInits)
    {
        if (init.second)
            init.second();
    }
}

SocketGlobals::Ini& SocketGlobals::ini()
{
    return s_instance->m_impl->m_ini;
}

SocketGlobals::DebugIni& SocketGlobals::debugIni()
{
    return s_instance->m_impl->m_debugIni;
}

aio::AIOService& SocketGlobals::aioService()
{
    return s_instance->m_impl->m_aioServiceGuard.aioService();
}

cloud::AddressResolver& SocketGlobals::addressResolver()
{
    return *s_instance->m_impl->m_addressResolver;
}

cloud::MediatorAddressPublisher& SocketGlobals::addressPublisher()
{
    return *s_instance->m_impl->m_addressPublisher;
}

hpm::api::MediatorConnector& SocketGlobals::mediatorConnector()
{
    return *s_instance->m_impl->m_mediatorConnector;
}

cloud::OutgoingTunnelPool& SocketGlobals::outgoingTunnelPool()
{
    return *s_instance->m_impl->m_outgoingTunnelPool;
}

cloud::CloudConnectSettings& SocketGlobals::cloudConnectSettings()
{
    return s_instance->m_impl->m_cloudConnectSettings;
}

cloud::tcp::ReverseConnectionPool& SocketGlobals::tcpReversePool()
{
    return *s_instance->m_impl->m_tcpReversePool;
}

int SocketGlobals::initializationFlags()
{
    return s_instance->m_impl->m_initializationFlags;
}

void SocketGlobals::init(int initializationFlags)
{
    QnMutexLocker lock(&s_mutex);

    if (++s_counter == 1) //< First in.
    {
        s_initState = InitState::inintializing; //< Allow creating Pollable(s) in constructor.
        s_instance = new SocketGlobals(initializationFlags);

        // TODO: #ak disable cloud based on m_initializationFlags.
        s_instance->initializeCloudConnectivity();

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
        "  --enforce-ssl                    Use ssl for every connection" << std::endl <<
        "  --enforce-mediator={endpoint}    Enforces custom mediator address" << std::endl <<
        "  --cloud-connect-disable-udp      Disable UDP hole punching" << std::endl <<
        "  --cloud-connect-enable-proxy-only" << std::endl;
}

void SocketGlobals::applyArguments(const utils::ArgumentParser& arguments)
{
    if (const auto value = arguments.get("ip-version", "ip"))
        SocketFactory::setIpVersion(*value);

    if (const auto value = arguments.get("enforce-socket", "socket"))
        SocketFactory::enforceStreamSocketType(*value);

    if (arguments.get("enforce-ssl", "ssl"))
        SocketFactory::enforceSsl();

    if (const auto value = arguments.get("enforce-mediator", "mediator"))
        mediatorConnector().mockupMediatorUrl(*value);

    if (arguments.get("cloud-connect-disable-udp"))
    {
        cloud::ConnectorFactory::setEnabledCloudConnectMask(
            cloud::ConnectorFactory::getEnabledCloudConnectMask() &
            ~((int)cloud::CloudConnectType::udpHp));
    }

    if (arguments.get("cloud-connect-enable-proxy-only"))
    {
        cloud::ConnectorFactory::setEnabledCloudConnectMask(
            (int)cloud::CloudConnectType::proxy);
    }
}

void SocketGlobals::customInit(CustomInit init, CustomDeinit deinit)
{
    QnMutexLocker lock(&s_instance->m_impl->m_mutex);
    if (s_instance->m_impl->m_customInits.emplace(init, deinit).second)
        init();
}

void SocketGlobals::setDebugIniReloadTimer()
{
    m_impl->m_debugIniReloadTimer->start(
        kDebugIniReloadInterval,
        [this]()
        {
        m_impl->m_debugIni.reload();
            setDebugIniReloadTimer();
        });
}

void SocketGlobals::initializeCloudConnectivity()
{
    m_impl->m_mediatorConnector = std::make_unique<hpm::api::MediatorConnector>();
    m_impl->m_addressPublisher = std::make_unique<cloud::MediatorAddressPublisher>(
        m_impl->m_mediatorConnector->systemConnection());
    m_impl->m_outgoingTunnelPool = std::make_unique<cloud::OutgoingTunnelPool>();
    m_impl->m_tcpReversePool = std::make_unique<cloud::tcp::ReverseConnectionPool>(
        m_impl->m_mediatorConnector->clientConnection());
    m_impl->m_addressResolver = std::make_unique<cloud::AddressResolver>(
        m_impl->m_mediatorConnector->clientConnection());
    m_impl->m_debugIniReloadTimer = std::make_unique<aio::Timer>();
}

QnMutex SocketGlobals::s_mutex;
std::atomic<SocketGlobals::InitState> SocketGlobals::s_initState(SocketGlobals::InitState::none);
size_t SocketGlobals::s_counter(0);
SocketGlobals* SocketGlobals::s_instance(nullptr);

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

    if (initializePeerId)
        SocketGlobals::outgoingTunnelPool().assignOwnPeerId("re", QnUuid::createUuid());
}

} // namespace network
} // namespace nx
