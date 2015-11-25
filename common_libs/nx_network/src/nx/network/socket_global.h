#include <nx/tool/log/log.h>

#include "aio/aioservice.h"

#include "cloud_connectivity/address_resolver.h"
#include "cloud_connectivity/mediator_connector.h"

namespace nx {

class NX_NETWORK_API SocketGlobals
{
public:
    inline static
    aio::AIOService& aioService()
    { return instance().m_aioService; }

    inline static
    cc::AddressResolver& addressResolver()
    { return instance().m_addressResolver; }

    inline static
    cc::MediatorConnector& mediatorConnector()
    { return instance().m_mediatorConnector; }

private:
    SocketGlobals();
    static SocketGlobals& instance();

    static std::once_flag s_onceFlag;
    static std::unique_ptr< SocketGlobals > s_instance;

private:
    std::shared_ptr< QnLog::Logs > m_log;
    aio::AIOService m_aioService;
    cc::AddressResolver m_addressResolver;
    cc::MediatorConnector m_mediatorConnector;
};

} // namespace nx
