#include "vms_gateway_embeddable.h"
#include "vms_gateway_process.h"


namespace nx {
namespace cloud {
namespace gateway {

VmsGatewayEmbeddable::VmsGatewayEmbeddable()
{
    addArg("/path/to/bin");
    addArg("-e");
    addArg("-general/listenOn", "127.0.0.1:0");
    addArg("-cloudConnect/allowIpTarget", "true");
    addArg("-http/connectSupport", "true");

    // do not allow VmsGateway reinit the log
    QnLog::s_disableLogConfiguration = true;

    if (startAndWaitUntilStarted())
    {
        auto endpoints = moduleInstance()->impl()->httpEndpoints();
        NX_ASSERT(endpoints.size() == 1);
        m_endpoint = std::move(endpoints.front());
    }
}

SocketAddress VmsGatewayEmbeddable::endpoint() const
{
    return m_endpoint;
}

} // namespace gateway
} // namespace cloud
} // namespace nx
