// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_gateway_embeddable.h"
#include "vms_gateway_process.h"

namespace nx {
namespace cloud {
namespace gateway {

VmsGatewayEmbeddable::VmsGatewayEmbeddable(
    bool isSslEnabled,
    const QString& certPath)
    :
    m_isSslEnabled(isSslEnabled)
{
    addArg("/path/to/bin");
    addArg("-e");
    addArg("-general/listenOn", "127.0.0.1:0");
    addArg("-cloudConnect/allowIpTarget", "true");
    addArg("-http/allowTargetEndpointInUrl", "true");
    addArg("-http/connectSupport", "true");
    addArg("-cloudConnect/tcpReversePoolSize", "0");
    addArg("-cloudConnect/preferedSslMode", "followIncomingConnection");

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

    if (startAndWaitUntilStarted())
    {
        auto endpoints = moduleInstance()->impl()->httpEndpoints();
        NX_ASSERT(endpoints.size() == 1);
        m_endpoint = std::move(endpoints.front());
        NX_DEBUG(this, "VmsGatewayEmbeddable has started on %1", m_endpoint);
    }
}

bool VmsGatewayEmbeddable::isSslEnabled() const
{
    return m_isSslEnabled;
}

network::SocketAddress VmsGatewayEmbeddable::endpoint() const
{
    return m_endpoint;
}

void VmsGatewayEmbeddable::enforceSslFor(const network::SocketAddress& targetAddress, bool enabled)
{
    moduleInstance()->impl()->enforceSslFor(targetAddress, enabled);
}

void VmsGatewayEmbeddable::enforceHeadersFor(
    const network::SocketAddress& targetAddress, network::http::HttpHeaders headers)
{
    moduleInstance()->impl()->enforceHeadersFor(targetAddress, std::move(headers));
}

std::map<uint16_t, uint16_t> VmsGatewayEmbeddable::forward(
    const nx::network::SocketAddress& target,
    const std::set<uint16_t>& targetPorts,
    const nx::network::ssl::AdapterFunc& certificateCheck)
{
    return moduleInstance()->impl()->forward(target, targetPorts, certificateCheck);
}

void VmsGatewayEmbeddable::beforeModuleStart()
{
    moduleInstance()->impl()->setEnableLoggingInitialization(false);
}

} // namespace gateway
} // namespace cloud
} // namespace nx
