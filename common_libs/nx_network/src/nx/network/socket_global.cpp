
#include "socket_global.h"

#include <nx/utils/std/future.h>

const std::chrono::seconds kReloadDebugConfigurationInterval(10);

namespace nx {
namespace network {

bool SocketGlobals::Config::isAddressDisabled(const HostAddress& address) const
{
    if (SocketGlobals::s_initState != InitState::done)
        return false;

    // Here 'static const' is an optimization as reload is called only on start.
    static const auto disabledIps = [this]()
    {
        std::list<QRegExp> addresses;
        const auto disabledAddressesString = QString::fromUtf8(disableAddresses);
        if (!disabledAddressesString.isEmpty())
        {
            for (const auto& s: disabledAddressesString.split(QChar(',')))
                addresses.push_back(QRegExp(s, Qt::CaseInsensitive, QRegExp::Wildcard));
        }
        return addresses;
    }();

    for (const auto& r: disabledIps)
    {
        if (r.exactMatch(address.toString()))
            return true;
    }

    return false;
}

SocketGlobals::SocketGlobals():
    m_log(QnLog::logs()),
    m_mediatorConnector(new hpm::api::MediatorConnector),
    m_addressPublisher(m_mediatorConnector->systemConnection()),
    m_tcpReversePool(m_mediatorConnector->clientConnection())
{
    m_addressResolver.reset(new cloud::AddressResolver(m_mediatorConnector->clientConnection()));
}

SocketGlobals::~SocketGlobals()
{
    // NOTE: can be move out into QnStoppableAsync::pleaseStop,
    //       what does not make any sense so far

    nx::utils::promise< void > promise;
    {
        utils::BarrierHandler barrier([&](){ promise.set_value(); });
        m_debugConfigurationTimer.pleaseStop(barrier.fork());
        m_addressResolver->pleaseStop(barrier.fork());
        m_addressPublisher.pleaseStop(barrier.fork());
        m_mediatorConnector->pleaseStop(barrier.fork());
        m_outgoingTunnelPool.pleaseStop(barrier.fork());
        m_tcpReversePool.pleaseStop(barrier.fork());
    }

    promise.get_future().wait();
    m_mediatorConnector.reset();

    for (const auto& init: m_customInits)
    {
        if (init.second)
            init.second();
    }
}

void SocketGlobals::init()
{
    QnMutexLocker lock(&s_mutex);
    if (++s_counter == 1) // first in
    {
        s_initState = InitState::inintializing;
        s_instance = new SocketGlobals;
        s_initState = InitState::done;

        lock.unlock();
        s_instance->setDebugConfigurationTimer();
    }
}

void SocketGlobals::deinit()
{
    QnMutexLocker lock(&s_mutex);
    if (--s_counter == 0) // last out
    {
        delete s_instance;
        s_initState = InitState::deinitializing;
        s_instance = nullptr;
        s_initState = InitState::none; // allow creating Pollable(s) in destructor
    }
}

void SocketGlobals::verifyInitialization()
{
    NX_CRITICAL(
        s_initState != InitState::none,
        "SocketGlobals::InitGuard must be initialized before using Sockets");
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
    m_debugConfigurationTimer.start(
        kReloadDebugConfigurationInterval,
        [this]()
        {
            m_debugConfiguration.reload(utils::FlagConfig::OutputType::silent);
            setDebugConfigurationTimer();
        });
}

QnMutex SocketGlobals::s_mutex;
std::atomic<SocketGlobals::InitState> SocketGlobals::s_initState(SocketGlobals::InitState::none);
size_t SocketGlobals::s_counter(0);
SocketGlobals* SocketGlobals::s_instance(nullptr);

} // namespace network
} // namespace nx
