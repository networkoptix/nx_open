#include "relay_api_client_over_http_upgrade.h"

#include <nx/network/http/custom_headers.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/type_utils.h>

#include "relay_api_http_paths.h"

namespace nx::cloud::relay::api {

ClientOverHttpUpgrade::ClientOverHttpUpgrade(
    const QUrl& baseUrl,
    ClientFeedbackFunction feedbackFunction)
    :
    base_type(baseUrl, std::move(feedbackFunction))
{
}

void ClientOverHttpUpgrade::beginListening(
    const std::string& peerName,
    BeginListeningHandler completionHandler)
{
    using namespace std::placeholders;

    BeginListeningRequest request;
    request.peerName = peerName;

    issueUpgradeRequest<
        BeginListeningRequest, const char*, BeginListeningHandler, BeginListeningResponse>(
            nx_http::Method::post,
            kRelayProtocolName,
            std::move(request),
            kServerIncomingConnectionsPath,
            {peerName.c_str()},
            [this, completionHandler = std::move(completionHandler)](
                ResultCode resultCode,
                BeginListeningResponse response,
                std::unique_ptr<AbstractStreamSocket> connection)
            {
                giveFeedback(resultCode);
                completionHandler(
                    resultCode,
                    std::move(response),
                    std::move(connection));
            });
}

void ClientOverHttpUpgrade::openConnectionToTheTargetHost(
    const std::string& sessionId,
    OpenRelayConnectionHandler completionHandler)
{
    using namespace std::placeholders;

    ConnectToPeerRequest request;
    request.sessionId = sessionId;

    issueUpgradeRequest<
        ConnectToPeerRequest, const char*, OpenRelayConnectionHandler>(
            nx_http::Method::post,
            kRelayProtocolName,
            std::move(request),
            kClientSessionConnectionsPath,
            {sessionId.c_str()},
            [this, completionHandler = std::move(completionHandler)](
                ResultCode resultCode,
                std::unique_ptr<AbstractStreamSocket> connection)
            {
                giveFeedback(resultCode);
                completionHandler(resultCode, std::move(connection));
            });
}

} // namespace nx::cloud::relay::api
