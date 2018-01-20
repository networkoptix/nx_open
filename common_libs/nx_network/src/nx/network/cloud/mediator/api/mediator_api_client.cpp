#include "mediator_api_client.h"

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/http_client.h>
#include <nx/network/url/url_builder.h>

#include "mediator_api_http_paths.h"

namespace nx {
namespace hpm {
namespace api {

Client::Client(const QUrl& baseMediatorApiUrl):
    m_baseMediatorApiUrl(baseMediatorApiUrl)
{
}

std::tuple<nx_http::StatusCode::Value, ListeningPeers> Client::getListeningPeers() const
{
    auto requestUrl = nx::network::url::Builder(m_baseMediatorApiUrl)
        .appendPath(kStatisticsListeningPeersPath).toUrl();

    nx_http::HttpClient httpClient;
    if (!httpClient.doGet(requestUrl))
    {
        return std::make_tuple(
            nx_http::StatusCode::serviceUnavailable,
            ListeningPeers());
    }

    if (httpClient.response()->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        return std::make_tuple(
            static_cast<nx_http::StatusCode::Value>(
                httpClient.response()->statusLine.statusCode),
            ListeningPeers());
    }

    QByteArray responseBody;
    while (!httpClient.eof())
        responseBody += httpClient.fetchMessageBodyBuffer();

    return std::make_tuple(
        nx_http::StatusCode::ok,
        QJson::deserialized<api::ListeningPeers>(responseBody));
}

} // namespace api
} // namespace hpm
} // namespace nx
