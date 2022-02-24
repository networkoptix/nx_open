// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_common.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/module_instance_launcher.h>

#include "../vms_gateway_process_public.h"

namespace nx {
namespace cloud {
namespace gateway {

class NX_VMS_GATEWAY_API VmsGatewayFunctionalTest:
    public utils::test::ModuleLauncher<VmsGatewayProcessPublic>
{
public:
    enum Flags
    {
        doNotReinitialiseSocketGlobals = 1,
    };

    VmsGatewayFunctionalTest(int flags = 0);
    virtual ~VmsGatewayFunctionalTest();

    virtual bool startAndWaitUntilStarted() override;
    bool startAndWaitUntilStarted(
        bool allowIpTarget, bool proxyTargetPort, bool connectSupport,
        std::optional<std::chrono::seconds> sendTimeout = {},
        std::optional<std::chrono::seconds> recvTimeout = {});

    network::SocketAddress endpoint() const;
    const std::unique_ptr<nx::network::http::TestHttpServer>& testHttpServer() const;

    /** Test will use path to store all of its files. */
    static void setTemporaryDirectoryPath(const QString& path);

private:
    QString m_tmpDir;
    int m_httpPort;
    std::unique_ptr<nx::network::http::TestHttpServer> m_testHttpServer;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
