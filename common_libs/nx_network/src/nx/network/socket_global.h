#ifndef NX_NETWORK_SOCKET_GLOBAL_H
#define NX_NETWORK_SOCKET_GLOBAL_H

#include <nx/utils/log/log.h>

#include "aio/aioservice.h"

#include "cloud/address_resolver.h"
#include "cloud/mediator_address_publisher.h"
#include "cloud/mediator_connector.h"
#include "cloud/cloud_tunnel.h"

namespace nx {
namespace network {

class NX_NETWORK_API SocketGlobals
{
public:
    inline static
    aio::AIOService& aioService()
    { return s_instance->m_aioService; }

    inline static
    cloud::AddressResolver& addressResolver()
    { return s_instance->m_addressResolver; }

    inline static
    cloud::MediatorAddressPublisher& addressPublisher()
    { return s_instance->m_addressPublisher; }

    inline static
    hpm::api::MediatorConnector& mediatorConnector()
    { return *s_instance->m_mediatorConnector; }

    inline static
    cloud::TunnelPool& tunnelPool()
    { return s_instance->m_cloudTunnelPool; }

	static void init();	/** Should be called before any socket use */
	static void deinit();  /** Should be called when sockets are not needed any more */

	typedef std::unique_ptr< int, decltype( deinit ) > Guard;
	static Guard initGuard();

	class InitGuard
	{
	public:
		InitGuard() { init(); }
		~InitGuard() { deinit(); }

	private:
		InitGuard( const InitGuard& );
		//InitGuard& oprator=( const InitGuard& );
	};

private:
    SocketGlobals();
    ~SocketGlobals();

	static QnMutex s_mutex;
	static size_t s_counter;
    static SocketGlobals* s_instance;

private:
    std::shared_ptr< QnLog::Logs > m_log;
    aio::AIOService m_aioService;

    std::unique_ptr<hpm::api::MediatorConnector> m_mediatorConnector;
    cloud::AddressResolver m_addressResolver;
    cloud::MediatorAddressPublisher m_addressPublisher;
    cloud::TunnelPool m_cloudTunnelPool;
};

} // namespace network
} // namespace nx

#endif  //NX_NETWORK_SOCKET_GLOBAL_H
