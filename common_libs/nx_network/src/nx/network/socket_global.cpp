#include "socket_global.h"

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

    std::promise< void > promise;
    {
        BarrierHandler barrier([&](){ promise.set_value(); });
        m_addressResolver.pleaseStop( barrier.fork() );
        m_addressPublisher.pleaseStop( barrier.fork() );
        m_mediatorConnector->pleaseStop( barrier.fork() );
        m_outgoingTunnelPool.pleaseStop( barrier.fork() );
    }

    promise.get_future().wait();
    m_mediatorConnector.reset();
}

void SocketGlobals::init()
{
	if( ++s_counter == 1 )
        s_instance = new SocketGlobals;
}

void SocketGlobals::deinit()
{
    if( --s_counter == 0 )
        delete s_instance;
}

void SocketGlobals::check()
{
    NX_CRITICAL(
        s_counter.load() != 0,
        "SocketGlobals::InitGuard shell be initialized before using Sockets");
}

std::atomic<int> SocketGlobals::s_counter( 0 );
SocketGlobals* SocketGlobals::s_instance( nullptr );

} // namespace network
} // namespace nx
