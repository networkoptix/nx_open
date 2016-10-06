
#include "socket_global.h"

#include <nx/utils/std/future.h>

std::chrono::seconds kReloadDebugConfigurationInterval(10);

namespace nx {
namespace network {

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
        s_isInitialized = true; // allow creating Pollable(s) in constructor
        s_instance = new SocketGlobals;

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
        s_instance = nullptr;
        s_isInitialized = false; // allow creating Pollable(s) in destructor
    }
}

void SocketGlobals::verifyInitialization()
{
    NX_CRITICAL(
        s_isInitialized,
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
            m_debugConfiguration.reload(false); // silent reload
            setDebugConfigurationTimer();
        });
}

QnMutex SocketGlobals::s_mutex;
std::atomic<bool> SocketGlobals::s_isInitialized(false);
size_t SocketGlobals::s_counter(0);
SocketGlobals* SocketGlobals::s_instance(nullptr);

} // namespace network
} // namespace nx
