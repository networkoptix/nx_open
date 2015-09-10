#include "cloud_connectivity/address_resolver.h"
#include "aio/aioservice.h"

namespace nx {

class SocketGlobals
{
public:
    inline static
    cc::AddressResolver& addressResolver()
    { return instance().m_addressResolver; }

    inline static
    aio::AIOService& aioService()
    { return instance().m_aioService; }

private:
    static SocketGlobals& instance();

    static QnMutex s_mutex;
    static std::unique_ptr< SocketGlobals > s_instance;

private:
    cc::AddressResolver m_addressResolver;
    aio::AIOService m_aioService;
};

} // namespace nx
