#include "http_client.h"

#include <utility>
#include <system_error>

#include <nx/utils/log/log_message.h>

#include <QtCore/QString>

#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::utils;
using namespace nx::network;

HttpClient::HttpClient()
{
    setAuthType(http::AuthType::authBasic);

    // masquarade as curl since vivotek rejects requests with default user agent on some endpoints
    addAdditionalHeader("User-Agent", "curl/7.69.1");
}

cf::future<http::BufferType> HttpClient::get(const Url& url)
{
    return Client::get(url)
        .then_unwrap([this](auto&& response) { return processResponse(std::move(response)); })
        .then(addExceptionContextAndRethrow("HTTP GET %1 failed", url));
}

cf::future<http::BufferType> HttpClient::post(const Url& url,
    http::StringType contentType, http::BufferType requestBody)
{
    return Client::post(url, std::move(contentType), std::move(requestBody))
        .then_unwrap([this](auto&& response) { return processResponse(std::move(response)); })
        .then(addExceptionContextAndRethrow("HTTP POST %1 failed", url));
}

http::BufferType HttpClient::processResponse(http::Response&& response)
{
    int expectedStatusCode = http::StatusCode::ok;
    if (request().headers.count("Upgrade"))
        expectedStatusCode = http::StatusCode::switchingProtocols;

    const int statusCode = response.statusLine.statusCode;
    if (statusCode == expectedStatusCode)
        return std::move(response.messageBody);

    switch (statusCode)
    {
        case http::StatusCode::unauthorized:
        case http::StatusCode::forbidden:
            throw Exception(ErrorCode::unauthorized,
                "Response status %1, expected %2", statusCode, expectedStatusCode);

        default:
            throw Exception("Response status %1, expected %2", statusCode, expectedStatusCode);
    }
}

} // namespace nx::vms_server_plugins::analytics::vivotek
