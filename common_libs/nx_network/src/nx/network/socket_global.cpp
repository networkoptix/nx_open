
#include "socket_global.h"

#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>

const std::chrono::seconds kReloadDebugConfigurationInterval(10);

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

SocketGlobals::SocketGlobals():
    m_log(QnLog::logs())
{
}

SocketGlobals::~SocketGlobals()
{
    // NOTE: can be move out into QnStoppableAsync::pleaseStop,
    //       what does not make any sense so far

    nx::utils::promise< void > promise;
    {
        utils::BarrierHandler barrier([&](){ promise.set_value(); });
        if (m_debugConfigurationTimer)
            m_debugConfigurationTimer->pleaseStop(barrier.fork());
        m_addressResolver->pleaseStop(barrier.fork());
        m_addressPublisher->pleaseStop(barrier.fork());
        //m_mediatorConnector->pleaseStop(barrier.fork());
        m_outgoingTunnelPool->pleaseStop(barrier.fork());
        m_tcpReversePool->pleaseStop(barrier.fork());
    }

    promise.get_future().wait();
    //m_mediatorConnector.reset();
    //auto mediatorConnectorGuard = makeScopedGuard(
    //    [this]()
    //    {
    //        m_mediatorConnector->pleaseStopSync(false);
    //    });

    for (const auto& init: m_customInits)
    {
        if (init.second)
            init.second();
    }
}

void SocketGlobals::init()
{
    QnMutexLocker lock(&s_mutex);
    if (++s_counter == 1) //< First in.
    {
        s_initState = InitState::inintializing; //< Allow creating Pollable(s) in constructor.
        s_instance = new SocketGlobals;
        
        s_instance->initializeCloudConnectivity();

        s_initState = InitState::done;

        lock.unlock();
        s_instance->setDebugConfigurationTimer();
    }
}

void SocketGlobals::deinit()
{
    QnMutexLocker lock(&s_mutex);
    if (--s_counter == 0) //< Last out.
    {
        delete s_instance;
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
    if (const auto value = arguments.get("enforce-mediator", "mediator"))
        mediatorConnector().mockupAddress(*value);

    if (const auto value = arguments.get("enforce-socket", "socket"))
        SocketFactory::enforceStreamSocketType(*value);

    if (arguments.get("enforce-ssl", "ssl"))
        SocketFactory::enforceSsl();
}

void SocketGlobals::customInit(CustomInit init, CustomDeinit deinit)
{
    QnMutexLocker lock(&s_instance->m_mutex);
    if (s_instance->m_customInits.emplace(init, deinit).second)
        init();
}

void SocketGlobals::setDebugConfigurationTimer()
{
    m_debugConfigurationTimer = std::make_unique<aio::Timer>();
    m_debugConfigurationTimer->start(
        kReloadDebugConfigurationInterval,
        [this]()
        {
            m_debugConfiguration.reload(utils::FlagConfig::OutputType::silent);
            setDebugConfigurationTimer();
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
}

QnMutex SocketGlobals::s_mutex;
std::atomic<SocketGlobals::InitState> SocketGlobals::s_initState(SocketGlobals::InitState::none);
size_t SocketGlobals::s_counter(0);
SocketGlobals* SocketGlobals::s_instance(nullptr);

} // namespace network
} // namespace nx
