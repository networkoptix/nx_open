#include <nx/network/ssl_socket.h>
#include <nx/network/socket_global.h>

#define USE_GMOCK
#include <nx/utils/test_support/run_test.h>

int main(int argc, char **argv)
{
    return nx::utils::runTest(
        argc, argv,
        [](const nx::utils::ArgumentParser&)
        {
            const auto sslCert = nx::network::SslEngine::makeCertificateAndKey(
                "nx_network_ut", "US", "Network Optix");

            NX_CRITICAL(!sslCert.isEmpty());
            nx::network::SslEngine::useCertificateAndPkey(sslCert);
        });
}
