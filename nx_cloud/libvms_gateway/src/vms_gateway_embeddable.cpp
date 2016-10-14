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
    addArg("-cloudConnect/tcpReversePoolSize", "0");

    if (isSslEnabled)
    {
        addArg("-http/sslSupport", "true");
        if (!certPath.isEmpty())
            addArg("-http/sslCertPath", certPath.toUtf8());
    }
    else
    {
        addArg("-http/sslSupport", "false");
    }

    // do not allow VmsGateway reinit the log
    addArg("-log/logLevel", "none");

    if (startAndWaitUntilStarted())
    {
        auto endpoints = moduleInstance()->impl()->httpEndpoints();
        NX_ASSERT(endpoints.size() == 1);
        m_endpoint = std::move(endpoints.front());
        qDebug() << "VmsGatewayEmbeddable has started on" << m_endpoint.toString();
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

void VmsGatewayEmbeddable::enforceSslFor(const SocketAddress& targetAddress, bool enabled)
{
    moduleInstance()->impl()->enforceSslFor(targetAddress, enabled);
}

} // namespace gateway
} // namespace cloud
} // namespace nx
