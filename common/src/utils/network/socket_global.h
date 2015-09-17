#include "utils/common/log.h"

#include "aio/aioservice.h"

#include "cloud_connectivity/address_resolver.h"
#include "cloud_connectivity/cloud_connection_info.h"

namespace nx {

class SocketGlobals
{
public:
    inline static
    aio::AIOService& aioService()
    { return instance().m_aioService; }

    inline static
    cc::AddressResolver& addressResolver()
    { return instance().m_addressResolver; }

    inline static
    cc::CloudConnectionInfo& cloudInfo()
    { return instance().m_cloudConnectionInfo; }

private:
    SocketGlobals();
    static SocketGlobals& instance();

    static std::once_flag s_onceFlag;
    static std::unique_ptr< SocketGlobals > s_instance;

private:
    std::shared_ptr< QnLog::Logs > m_log;
    cc::AddressResolver m_addressResolver;
    aio::AIOService m_aioService;
    cc::CloudConnectionInfo m_cloudConnectionInfo;
};

} // namespace nx
