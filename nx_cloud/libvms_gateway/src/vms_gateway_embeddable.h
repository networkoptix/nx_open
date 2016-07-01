#pragma once

#include "vms_gateway_process_public.h"

#include <nx/network/socket_common.h>
#include <nx/utils/singleton.h>
#include <nx/utils/test_support/module_instance_launcher.h>


namespace nx {
namespace cloud {
namespace gateway {

class VmsGatewayEmbeddable
:
    public QObject,
    public Singleton<VmsGatewayEmbeddable>,
    private utils::test::ModuleLauncher<VmsGatewayProcessPublic>
{
    Q_OBJECT

public:
    VmsGatewayEmbeddable();

    SocketAddress endpoint() const;

private:
    SocketAddress m_endpoint;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
