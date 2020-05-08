#include "http_client.h"

#include <utility>
#include <stdexcept>
#include <system_error>

#include <nx/utils/log/log_message.h>
#include <nx/network/http/buffer_source.h>

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

cf::future<http::BufferType> HttpClient::get(Url url)
{
    cf::promise<cf::unit> promise;
    auto future = promise.get_future();

    doGet(std::move(url), toHandler(std::move(promise)));

    return future
        .then(translateBrokenPromiseToOperationCancelled)
        .then_ok([this](auto&&) { return processResponse(); })
        .then(addExceptionContext("HTTP GET %1 failed", std::move(url)));
}

cf::future<http::BufferType> HttpClient::post(Url url,
    http::StringType contentType, http::BufferType requestBody)
{
    cf::promise<cf::unit> promise;
    auto future = promise.get_future();

    setRequestBody(std::make_unique<http::BufferSource>(
        std::move(contentType), std::move(requestBody)));
    doPost(std::move(url), toHandler(std::move(promise)));

    return future
        .then(translateBrokenPromiseToOperationCancelled)
        .then_ok([this](auto&&) { return processResponse(); })
        .then(addExceptionContext("HTTP POST %1 failed", std::move(url)));
}

http::BufferType HttpClient::processResponse()
{
    if (failed())
        throw std::system_error(lastSysErrorCode(), std::system_category());

    auto expectedStatusCode = http::StatusCode::ok;
    if (request().headers.count("Upgrade"))
        expectedStatusCode = http::StatusCode::switchingProtocols;

    const auto statusCode = response()->statusLine.statusCode;
    if (statusCode == expectedStatusCode)
        return fetchMessageBodyBuffer();

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
