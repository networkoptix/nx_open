#include "vms_gateway_embeddable.h"
#include "vms_gateway_process.h"


namespace nx {
namespace cloud {
namespace gateway {

VmsGatewayEmbeddable::VmsGatewayEmbeddable(
    bool isSslEnabled, const QString& certPath)
:
    m_isSslEnabled(isSslEnabled)
{
    addArg("/path/to/bin");
    addArg("-e");
    addArg("-general/listenOn", "127.0.0.1:0");
    addArg("-cloudConnect/allowIpTarget", "true");
    addArg("-http/allowTargetEndpointInUrl", "true");
    //addArg("-http/connectSupport", "true");

    if (isSslEnabled)
    {
        addArg("-http/sslSupport", "true");
        if (!certPath.isEmpty())
            addArg("-http/sslCertPath", certPath.toUtf8());
    }

    // do not allow VmsGateway reinit the log
    addArg("-log/logLevel", "none");

    if (startAndWaitUntilStarted())
    {
        auto endpoints = moduleInstance()->impl()->httpEndpoints();
        NX_ASSERT(endpoints.size() == 1);
        m_endpoint = std::move(endpoints.front());
    }
}

bool VmsGatewayEmbeddable::isSslEnabled() const
{
    return m_isSslEnabled;
}

SocketAddress VmsGatewayEmbeddable::endpoint() const
{
    return m_endpoint;
}

} // namespace gateway
} // namespace cloud
} // namespace nx
