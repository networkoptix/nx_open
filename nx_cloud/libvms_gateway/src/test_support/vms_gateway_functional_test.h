#pragma once

#include <vector>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/network/http/test_http_server.h>
#include <nx/network/socket_common.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/test_support/module_instance_launcher.h>

#include "vms_gateway_process_public.h"

namespace nx {
namespace cloud {
namespace gateway {

class VmsGatewayFunctionalTest:
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
        bool allowIpTarget, bool proxyTargetPort, bool connectSupport);

    SocketAddress endpoint() const;
    const std::unique_ptr<TestHttpServer>& testHttpServer() const;

    /** Test will use path to store all of its files. */
    static void setTemporaryDirectoryPath(const QString& path);

private:
    QString m_tmpDir;
    int m_httpPort;
    std::unique_ptr<TestHttpServer> m_testHttpServer;
};

} // namespace gateway
} // namespace cloud
} // namespace nx
