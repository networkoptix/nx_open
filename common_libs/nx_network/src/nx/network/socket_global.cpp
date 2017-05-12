
#include "socket_global.h"

#include <nx/utils/thread/barrier_handler.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>

#include "aio/pollset_factory.h"

const std::chrono::seconds kReloadDebugConfigInterval(10);

namespace nx {
namespace network {

bool SocketGlobals::Config::isHostDisabled(const HostAddress& host) const
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
// SocketGlobals::AioServiceGuard

SocketGlobals::AioServiceGuard::AioServiceGuard()
{
}

SocketGlobals::AioServiceGuard::~AioServiceGuard()
{
    if (m_aioService)
        m_aioService->pleaseStopSync();
}

void SocketGlobals::AioServiceGuard::initialize()
{
    m_aioService = std::make_unique<aio::AIOService>();
}

aio::AIOService& SocketGlobals::AioServiceGuard::aioService()
{
    return *m_aioService;
}

//-------------------------------------------------------------------------------------------------
// SocketGlobals

SocketGlobals::SocketGlobals(int initializationFlags):
    m_initializationFlags(initializationFlags)
{
    if (m_initializationFlags & InitializationFlags::disableUdt)
        m_pollSetFactory.disableUdt();

    m_aioServiceGuard.initialize();
}

SocketGlobals::~SocketGlobals()
{
    // NOTE: should be moved to QnStoppableAsync::pleaseStop

    nx::utils::promise< void > cloudServicesStoppedPromise;
    {
        utils::BarrierHandler barrier([&](){ cloudServicesStoppedPromise.set_value(); });
        m_debugConfigTimer->pleaseStop(barrier.fork());
        m_addressResolver->pleaseStop(barrier.fork());
        m_addressPublisher->pleaseStop(barrier.fork());
        m_outgoingTunnelPool->pleaseStop(barrier.fork());
        m_tcpReversePool->pleaseStop(barrier.fork());
    }

    cloudServicesStoppedPromise.get_future().wait();

    for (const auto& init: m_customInits)
    {
        if (init.second)
            init.second();
    }
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
        s_instance->setDebugConfigTimer();
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

void SocketGlobals::applyArguments(const utils::ArgumentParser& arguments)
{
    if (const auto value = arguments.get("ip-version", "ip"))
        SocketFactory::setIpVersion(*value);

    if (const auto value = arguments.get("enforce-socket", "socket"))
        SocketFactory::enforceStreamSocketType(*value);

    if (arguments.get("enforce-ssl", "ssl"))
        SocketFactory::enforceSsl();

    if (const auto value = arguments.get("enforce-mediator", "mediator"))
        mediatorConnector().mockupAddress(*value);
}

void SocketGlobals::customInit(CustomInit init, CustomDeinit deinit)
{
    QnMutexLocker lock(&s_instance->m_mutex);
    if (s_instance->m_customInits.emplace(init, deinit).second)
        init();
}

void SocketGlobals::setDebugConfigTimer()
{
    m_debugConfigTimer->start(
        kReloadDebugConfigInterval,
        [this]()
        {
            m_debugConfig.reload(utils::FlagConfig::OutputType::silent);
            setDebugConfigTimer();
        });
}

void SocketGlobals::initializeCloudConnectivity()
{
    m_mediatorConnector = std::make_unique<hpm::api::MediatorConnector>();
    m_addressPublisher = std::make_unique<cloud::MediatorAddressPublisher>(
        m_mediatorConnector->systemConnection());
    m_outgoingTunnelPool = std::make_unique<cloud::OutgoingTunnelPool>();
    m_tcpReversePool = std::make_unique<cloud::tcp::ReverseConnectionPool>(
        m_mediatorConnector->clientConnection());
    m_addressResolver = std::make_unique<cloud::AddressResolver>(
        m_mediatorConnector->clientConnection());
    m_debugConfigTimer = std::make_unique<aio::Timer>();
}

QnMutex SocketGlobals::s_mutex;
std::atomic<SocketGlobals::InitState> SocketGlobals::s_initState(SocketGlobals::InitState::none);
size_t SocketGlobals::s_counter(0);
SocketGlobals* SocketGlobals::s_instance(nullptr);

} // namespace network
} // namespace nx
