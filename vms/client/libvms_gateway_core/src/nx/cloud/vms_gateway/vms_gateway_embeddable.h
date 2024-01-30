// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>

#include <nx/network/http/http_types.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/test_support/module_instance_launcher.h>

#include "vms_gateway_process_public.h"

namespace nx {
namespace cloud {
namespace gateway {

class NX_VMS_GATEWAY_API VmsGatewayEmbeddable:
    public QObject,
    private utils::test::ModuleLauncher<VmsGatewayProcessPublic>
{
    Q_OBJECT

public:
    VmsGatewayEmbeddable(
        bool isSslEnabled = true,
        const QString& certPath = {});

    bool isSslEnabled() const;
    network::SocketAddress endpoint() const;
    void enforceSslFor(const network::SocketAddress& targetAddress, bool enabled = true);
    void enforceHeadersFor(
        const network::SocketAddress& targetAddress, network::http::HttpHeaders headers);

    std::map<uint16_t, uint16_t> forward(
        const nx::network::SocketAddress& target,
        const std::set<uint16_t>& targetPorts,
        const nx::network::ssl::AdapterFunc& certificateCheck);

protected:
    virtual void beforeModuleStart() override;

private:
    const bool m_isSslEnabled;
    network::SocketAddress m_endpoint;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
