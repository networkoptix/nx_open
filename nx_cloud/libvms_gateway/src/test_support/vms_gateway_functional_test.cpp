/**********************************************************
* Dec 21, 2015
* akolesnikov
***********************************************************/

#include "vms_gateway_functional_test.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <sstream>
#include <thread>
#include <tuple>

#include <common/common_globals.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/httpclient.h>
#include <nx/network/socket.h>
#include <nx/network/socket_global.h>
#include <utils/common/cpp14.h>
#include <nx/utils/string.h>
#include <utils/common/sync_call.h>
#include <utils/crypt/linux_passwd_crypt.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical.h>

#include "vms_gateway_process.h"


namespace nx {
namespace cloud {
namespace gateway {

namespace {
QString sTemporaryDirectoryPath;
}

VmsGatewayFunctionalTest::VmsGatewayFunctionalTest()
:
    m_httpPort(0),
    m_testHttpServer(std::make_unique<TestHttpServer>())
{
    //starting clean test
    nx::network::SocketGlobalsHolder::instance()->reinitialize();

    m_tmpDir = 
        (sTemporaryDirectoryPath.isEmpty() ? QDir::homePath(): sTemporaryDirectoryPath) +
        "/vms_gateway_ut.data";
    QDir(m_tmpDir).removeRecursively();

    addArg("/path/to/bin");
    addArg("-e");
    addArg("-general/listenOn"); addArg("127.0.0.1:0");
    addArg("-general/dataDir"); addArg(m_tmpDir.toLatin1().constData());
    addArg("-log/logLevel"); addArg("DEBUG2");
}

VmsGatewayFunctionalTest::~VmsGatewayFunctionalTest()
{
    stop();

    QDir(m_tmpDir).removeRecursively();
}

bool VmsGatewayFunctionalTest::startAndWaitUntilStarted()
{
    return startAndWaitUntilStarted(true, true, true);
}

bool VmsGatewayFunctionalTest::startAndWaitUntilStarted(
    bool allowIpTarget, bool proxyTargetPort, bool connectSupport)
{
    if (!m_testHttpServer->bindAndListen())
        return false;

    if (allowIpTarget)
    {
        addArg("-cloudConnect/allowIpTarget");
        addArg("true");
    }

    if (proxyTargetPort)
    {
        addArg("-http/proxyTargetPort");
        addArg(QByteArray::number(
            m_testHttpServer->serverAddress().port).constData());
    }

    if (connectSupport)
    {
        addArg("-http/connectSupport");
        addArg("true");
    }

    if (!utils::test::ModuleLauncher<VmsGatewayProcessPublic>::startAndWaitUntilStarted())
        return false;

    const auto& httpEndpoints = moduleInstance()->impl()->httpEndpoints();
    if (httpEndpoints.empty())
        return false;
    m_httpPort = httpEndpoints.front().port;
    return true;
}

SocketAddress VmsGatewayFunctionalTest::endpoint() const
{
    return SocketAddress(HostAddress::localhost, m_httpPort);
}

const std::unique_ptr<TestHttpServer>& VmsGatewayFunctionalTest::testHttpServer() const
{
    return m_testHttpServer;
}

void VmsGatewayFunctionalTest::setTemporaryDirectoryPath(const QString& path)
{
    sTemporaryDirectoryPath = path;
}

}   // namespace gateway
}   // namespace cloud
}   // namespace nx
