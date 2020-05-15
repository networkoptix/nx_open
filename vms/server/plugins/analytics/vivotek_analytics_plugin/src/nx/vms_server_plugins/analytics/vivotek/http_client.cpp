#include "http_client.h"

#include <utility>
#include <stdexcept>
#include <system_error>

#include <nx/utils/log/log_message.h>

#include <QtCore/QString>

#include "exception_utils.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

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
        .then_ok([this](auto&& response) { return processResponse(std::move(response)); })
        .then(addExceptionContext("HTTP GET %1 failed", url));
}

cf::future<http::BufferType> HttpClient::post(const Url& url,
    http::StringType contentType, http::BufferType requestBody)
{
    return Client::post(url, std::move(contentType), std::move(requestBody))
        .then_ok([this](auto&& response) { return processResponse(std::move(response)); })
        .then(addExceptionContext("HTTP POST %1 failed", url));
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
            throw std::system_error(
                (int) std::errc::permission_denied, std::generic_category(),
                NX_FMT("Response status %1, expected %2", statusCode, expectedStatusCode)
                    .toStdString());

        default:
            throw std::runtime_error(
                NX_FMT("Response status %1, expected %2", statusCode, expectedStatusCode)
                    .toStdString());
    }
}

} // namespace nx::vms_server_plugins::analytics::vivotek
