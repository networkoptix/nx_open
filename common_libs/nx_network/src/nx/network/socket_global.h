#include <nx/utils/log/log.h>

#include "aio/aioservice.h"

#include "cloud_connectivity/address_resolver.h"
#include "cloud_connectivity/mediator_connector.h"

namespace nx {

class NX_NETWORK_API SocketGlobals
{
public:
    inline static
    aio::AIOService& aioService()
    { return s_instance->m_aioService; }

    inline static
    cc::AddressResolver& addressResolver()
    { return s_instance->m_addressResolver; }

    inline static
    cc::MediatorConnector& mediatorConnector()
    { return s_instance->m_mediatorConnector; }

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

	static QnMutex s_mutex;
	static size_t s_counter;
    static std::unique_ptr< SocketGlobals > s_instance;

private:
    std::shared_ptr< QnLog::Logs > m_log;
    aio::AIOService m_aioService;
    cc::AddressResolver m_addressResolver;
    cc::MediatorConnector m_mediatorConnector;
};

} // namespace nx
