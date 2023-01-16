// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "vms_gateway_process_public.h"

#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/test_support/module_instance_launcher.h>


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

protected:
    virtual void beforeModuleStart() override;

private:
    const bool m_isSslEnabled;
    network::SocketAddress m_endpoint;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
