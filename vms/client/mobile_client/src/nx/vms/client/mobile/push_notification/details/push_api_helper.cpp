// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_api_helper.h"

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/json.h>
#include <nx/utils/math/math.h>
#include <utils/common/delayed.h>

#include "push_platform_helpers.h"

namespace {

using namespace nx::vms::client::mobile::details;

NX_REFLECTION_ENUM_CLASS(MessageType,
    data,
    notification
)

struct SubscribePayload
{
    SubscribePayload();
    SubscribePayload(
        const SystemSet& systems,
        const TokenData& data);

    TokenProviderType provider;
    OptionalUserId userId;
    MessageType type;
    SystemSet systems;
    DeviceInfo deviceInfo;
};
NX_REFLECTION_INSTRUMENT(SubscribePayload, (provider)(userId)(type)(systems)(deviceInfo)) //<

SubscribePayload::SubscribePayload():
    provider(TokenProviderType::firebase), //< Firebase is used for compatibility with the older versions.
    type(nx::build_info::isIos() ? MessageType::notification : MessageType::data),
    systems(allSystemsModeValue()),
    deviceInfo(getDeviceInfo())
{
}

SubscribePayload::SubscribePayload(
    const SystemSet& systems,
    const TokenData& tokenData)
    :
    provider(tokenData.provider),
    userId(tokenData.userId),
    type(nx::build_info::isIos() ? MessageType::notification : MessageType::data),
    systems(systems),
    deviceInfo(getDeviceInfo())
{
}

//-------------------------------------------------------------------------------------------------

struct UnsubscribePayload
{
    UnsubscribePayload();
    UnsubscribePayload(const TokenData& data);

    TokenProviderType provider = TokenProviderType::firebase;
    OptionalUserId userId;
};
NX_REFLECTION_INSTRUMENT(UnsubscribePayload, (provider)(userId))

UnsubscribePayload::UnsubscribePayload():
    provider(TokenProviderType::firebase)
{
}

UnsubscribePayload::UnsubscribePayload(const TokenData& data):
    provider(data.provider),
    userId(data.userId)
{
}

//-------------------------------------------------------------------------------------------------

#define SubscribePayload_Fields (deviceInfo)(systems)(type)(provider)(userId)
#define UnsubscribePayload_Fields (provider)(userId)

//-------------------------------------------------------------------------------------------------

nx::utils::Url makeRequestUrl(const QString& token)
{
    const nx::utils::Url url = nx::network::url::Builder()
        .setScheme(nx::network::http::urlScheme(/*isSecure*/ true))
        .setHost(nx::branding::cloudHost())
        .setPath(QString("/api/notifications/subscriptions/%1").arg(token));
    return url;
}

void logRequest(
    const HttpClientPtr& client,
    const std::string& payload,
    const QString& method)
{
    static const auto kTemplate = QString("Sending %1 request %2 with payload %3");
    NX_DEBUG(client.get(), kTemplate.arg(method, client->url().toString(), payload.data()));
}

void logResponse(
    const HttpClientPtr& client,
    const QString& method,
    const int code,
    const bool success)
{
    static const auto kTemplate = QString("%1 request %2 is %3 with code %4%5");
    static const auto kResponseTemplate = QString(", response is %1");

    const auto requestText = client->url().toString();
    const auto rawResponse = client->fetchMessageBodyBuffer();
    const auto responseText = QString(rawResponse.empty()
        ? nullptr
        : kResponseTemplate.arg(rawResponse.data()));
    const auto codeText = QString::number(code);
    const auto successText = success ? "successful" : "failed";

    NX_DEBUG(client.get(), kTemplate.arg(method, requestText, successText, codeText, responseText));
}

void setPayload(const HttpClientPtr& client, const std::string& payload)
{
    if (!client || payload.empty())
        return;

    static const auto contentType = "application/json";

    const auto buffer = QByteArray::fromStdString(payload);
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> content(
        new nx::network::http::BufferSource(contentType, payload));
    client->setRequestBody(std::move(content));
}

using Handler = std::function<void (bool value)>;

nx::utils::MoveOnlyFunc<void ()> makeSafeCallback(
    const HttpClientPtr& client,
    PushApiHelper::Callback callback,
    const QString& requestMethod)
{
    const auto responseHandler =
        [thread = QThread::currentThread(), callback, client, requestMethod]()
        {
            if (!callback)
                return;

            using namespace nx::network::http;
            const auto response = client->response();
            const int code = response ? response->statusLine.statusCode : StatusCode::undefined;
            const bool success = qBetween<int>(StatusCode::ok, code, StatusCode::lastSuccessCode);

            logResponse(client, requestMethod, code, success);

            client->pleaseStopSync();
            executeInThread(thread,
                [callback, success, client]()
                {
                    // Since http client stores single callback for all requests
                    // as a member of the class and we add http client as parameter to the
                    // callback's lambda (to control lifetime of the client), we have to
                    // clear callback to prevent memory (clients) leaks.
                    client->setOnDone([]() {});
                    callback(success);
                });
        };

    return std::move(responseHandler);
}

} // namespace

namespace nx::vms::client::mobile::details {

HttpClientPtr PushApiHelper::createClient()
{
    return HttpClientPtr(
        new nx::network::http::AsyncClient(nx::network::ssl::kDefaultCertificateCheck));
}

void PushApiHelper::subscribe(
    const nx::network::http::Credentials& credentials,
    const TokenData& tokenData,
    const SystemSet& systems,
    const HttpClientPtr& client,
    Callback callback)
{
    if (credentials.authToken.empty() || !tokenData.isValid() || !client)
        return;

    static const auto kMethod = "PUT";
    const auto payload = nx::reflect::json::serialize(SubscribePayload(systems, tokenData));
    setPayload(client, payload);
    client->setCredentials(credentials);
    client->doPut(
        makeRequestUrl(tokenData.token),
        makeSafeCallback(client, callback, kMethod));

    logRequest(client, payload, kMethod);
}

void PushApiHelper::unsubscribe(
    const nx::network::http::Credentials& credentials,
    const TokenData& tokenData,
    const HttpClientPtr& client,
    Callback callback)
{
    if (credentials.authToken.empty() || !tokenData.isValid() || !client)
        return;

    static const auto kMethod = "DELETE";
    const auto payload = nx::reflect::json::serialize(UnsubscribePayload(tokenData));
    setPayload(client, payload);
    client->setCredentials(credentials);
    client->doDelete(
        makeRequestUrl(tokenData.token),
        makeSafeCallback(client, callback, kMethod));
    logRequest(client, payload, kMethod);
}

} // namespace nx::vms::client::mobile
