#include "tunneling_acceptance_tests.h"

namespace nx::network::http::tunneling::test {

TEST_P(HttpTunneling, tunnel_is_established)
{
    whenRequestTunnel();
    thenTunnelIsEstablished();
}

TEST_P(HttpTunneling, error_is_reported)
{
    stopTunnelingServer();

    whenRequestTunnel();
    thenTunnelIsNotEstablished();
}

TEST_P(HttpTunneling, timeout_supported)
{
    setEstablishTunnelTimeout(std::chrono::milliseconds(1));

    givenSilentTunnellingServer();

    whenRequestTunnel();

    thenTunnelIsNotEstablished();
    andResultCodeIs(SystemError::timedOut);
}

TEST_P(HttpTunneling, custom_http_headers_are_transferred)
{
    addSomeCustomHttpHeadersToTheClient();
    whenRequestTunnel();

    thenTunnelIsEstablished();
    andCustomHeadersAreReportedToTheServer();
}

//-------------------------------------------------------------------------------------------------

INSTANTIATE_TEST_CASE_P(
    EveryMethod,
    HttpTunneling,
    ::testing::Values(TunnelMethod::all));

INSTANTIATE_TEST_CASE_P(
    GetPostWithLargeMessageBody,
    HttpTunneling,
    ::testing::Values(TunnelMethod::getPost));

INSTANTIATE_TEST_CASE_P(
    ConnectionUpgrade,
    HttpTunneling,
    ::testing::Values(TunnelMethod::connectionUpgrade));

INSTANTIATE_TEST_CASE_P(
    Experimental,
    HttpTunneling,
    ::testing::Values(TunnelMethod::experimental));

INSTANTIATE_TEST_CASE_P(
    Ssl,
    HttpTunneling,
    ::testing::Values(TunnelMethod::ssl));

} // namespace nx::network::http::tunneling::test
