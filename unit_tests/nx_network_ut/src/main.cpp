/**********************************************************
* Dec 29, 2015
* akolesnikov
***********************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/network/ssl_socket.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <utils/common/command_line_parser.h>

int main(int  argc, char **argv)
{
    nx::network::SocketGlobals::InitGuard sgGuard;
    ::testing::InitGoogleMock(&argc, argv);

    nx::utils::ArgumentParser args(argc, argv);
    QnLog::applyArguments(args);
    nx::network::SocketGlobals::applyArguments(args);

    const auto sslCert = nx::network::SslEngine::makeCertificateAndKey(
        "test", "US", "Network Optix");

    NX_CRITICAL(!sslCert.isEmpty());
    nx::network::SslEngine::useCertificateAndPkey(sslCert);

    const int result = RUN_ALL_TESTS();
    return result;
}
