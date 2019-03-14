#include "test_relay.h"

#include <nx/network/cloud/tunnel/relay/api/detail/relay_api_client_over_http_upgrade.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/elapsed_timer.h>

#include "listen_on_relay_mode.h"

namespace nx {
namespace cctu {

void printTestRelayOptions(std::ostream* outStream)
{
    *outStream <<
        "Test relay mode:\n"
        "On success, process will exit and return 0\n"
        "  --test-relay                     Enable mode\n"
        "  --relay-url={URL}                E.g., http://relay.vmsproxy.com/\n"
        "  --listening-peer-host-name={globally unique hostname}. Optional\n";
}

std::tuple<nx::cloud::relay::api::ResultCode, std::unique_ptr<network::AbstractStreamSocket>>
    openConnectionToThePeer(
        const nx::utils::Url& baseRelayUrl,
        const std::string& listeningPeerName)
{
    using namespace nx::cloud::relay;

    api::detail::ClientOverHttpUpgrade relayClient(
        baseRelayUrl,
        nullptr);

    std::promise<std::tuple<api::ResultCode, std::string>> sessionDone;
    relayClient.startSession(
        std::string(),
        listeningPeerName,
        [&sessionDone](
            api::ResultCode resultCode,
            api::CreateClientSessionResponse response)
        {
            sessionDone.set_value(std::make_tuple(resultCode, response.sessionId));
        });
    auto [sessionResultCode, sessionId] = sessionDone.get_future().get();
    if (sessionResultCode != api::ResultCode::ok)
        return std::make_tuple(sessionResultCode, nullptr);

    std::promise<std::tuple<
        api::ResultCode,
        std::unique_ptr<network::AbstractStreamSocket>>
    > connectionEstablished;
    relayClient.openConnectionToTheTargetHost(
        sessionId,
        [&connectionEstablished](
            api::ResultCode resultCode,
            std::unique_ptr<network::AbstractStreamSocket> connection)
        {
            connectionEstablished.set_value(
                std::make_tuple(resultCode, std::move(connection)));
        });
    auto [connectionResultCode, connection] = 
        connectionEstablished.get_future().get();
    if (connectionResultCode != api::ResultCode::ok)
        return std::make_tuple(connectionResultCode, nullptr);

    return std::make_tuple(api::ResultCode::ok, std::move(connection));
}

std::unique_ptr<network::AbstractStreamSocket> openConnectionToThePeerWithRetries(
    const nx::utils::Url& baseRelayUrl,
    const std::string& listeningPeerName)
{
    using namespace nx::cloud::relay;

    const auto maxTimeToWait = std::chrono::seconds(20);

    std::unique_ptr<network::AbstractStreamSocket> connection;
    nx::utils::ElapsedTimer timer;
    timer.restart();
    nx::cloud::relay::api::ResultCode resultCode = 
        nx::cloud::relay::api::ResultCode::ok;
    while (!timer.hasExpired(maxTimeToWait))
    {
        std::tie(resultCode, connection) = openConnectionToThePeer(
            baseRelayUrl,
            listeningPeerName);
        if (connection)
            break;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!connection)
    {
        std::cerr << "Failed to open connection to host "
            << listeningPeerName << ": " << api::toString(resultCode) << std::endl;
    }

    return connection;
}

int testRelay(const nx::utils::ArgumentParser& args)
{
    ListenOnRelaySettings settings(args);
    TestHttpServerOnProxy server(settings);

    auto connection = openConnectionToThePeerWithRetries(
        settings.baseRelayUrl(),
        settings.listeningPeerHostName());
    if (!connection)
        return 1;

    nx::network::http::HttpClient httpClient(std::move(connection));
    if (!httpClient.doGet("/test") || !httpClient.response())
    {
        std::cerr << "Failed to perform HTTP request through relay"
            << std::endl;
        return 2;
    }

    const auto messageBody = httpClient.fetchEntireMessageBody();
    std::cout << "Received " << httpClient.response()->statusLine.statusCode
        << " response with body "
        << "\"" << (messageBody ? messageBody->toStdString() : std::string()) << "\""
        << std::endl;

    return 0;
}

} // namespace cctu
} // namespace nx
