
#include "socket_global.h"

#include <nx/utils/std/future.h>


namespace nx {
namespace network {

SocketGlobals::SocketGlobals()
    : m_log( QnLog::logs() )
    , m_mediatorConnector(new hpm::api::MediatorConnector)
    , m_addressResolver(m_mediatorConnector->clientConnection())
    , m_addressPublisher(m_mediatorConnector->systemConnection())
{
}

SocketGlobals::~SocketGlobals()
{
    // NOTE: can be move out into QnStoppableAsync::pleaseStop,
    //       what does not make any sense so far

    nx::utils::promise< void > promise;
    {
        utils::BarrierHandler barrier([&](){ promise.set_value(); });
        m_addressResolver.pleaseStop( barrier.fork() );
        m_addressPublisher.pleaseStop( barrier.fork() );
        m_mediatorConnector->pleaseStop( barrier.fork() );
        m_outgoingTunnelPool.pleaseStop( barrier.fork() );
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
    }
}

void SocketGlobals::deinit()
{
    QnMutexLocker lock(&s_mutex);
    if (--s_counter == 0) // last out
    {
        delete s_instance;
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
    if (const auto value = arguments.get(QLatin1String("enforce-mediator")))
        mediatorConnector().mockupAddress(*value);

    if (const auto value = arguments.get(QLatin1String("enforce-socket")))
        SocketFactory::enforceStreamSocketType(*value);

    if (arguments.get(QLatin1String("enforce-ssl")))
        SocketFactory::enforceSsl();

    if (const auto value = arguments.get<size_t>(QLatin1String("timeout-multiplier")))
        SocketFactory::setTimeoutMultiplier(*value);

    if (arguments.get(QLatin1String("disable-time-asserts")))
        SocketFactory::disableTimeAsserts();
}

void SocketGlobals::customInit(CustomInit init, CustomDeinit deinit)
{
    QnMutexLocker lock(&s_instance->m_mutex);
    if (s_instance->m_customInits.emplace(init, deinit).second)
    {
        lock.unlock();
        init();
    }
}

QnMutex SocketGlobals::s_mutex;
std::atomic<bool> SocketGlobals::s_isInitialized(false);
size_t SocketGlobals::s_counter(0);
SocketGlobals* SocketGlobals::s_instance(nullptr);

} // namespace network
} // namespace nx
