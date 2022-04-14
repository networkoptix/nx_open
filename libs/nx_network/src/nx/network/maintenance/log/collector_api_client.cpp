// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "collector_api_client.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>

namespace nx::network::maintenance::log {

CollectorApiClient::CollectorApiClient(const nx::utils::Url& baseApiUrl):
    m_baseApiUrl(baseApiUrl)
{
    bindToAioThread(getAioThread());
}

CollectorApiClient::~CollectorApiClient()
{
    pleaseStopSync();
}

void CollectorApiClient::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);
    m_httpClientPool.bindToAioThread(aioThread);
}

void CollectorApiClient::upload(
    const std::string& sessionId,
    nx::Buffer text,
    nx::utils::MoveOnlyFunc<void(Result)> handler)
{
    post(
        [this, sessionId, text = std::move(text), handler = std::move(handler)]() mutable
        {
            auto& ctx = m_httpClientPool.add(
                std::make_unique<http::AsyncClient>(ssl::kDefaultCertificateCheck),
                [this, handler = std::move(handler)](
                    std::unique_ptr<http::AsyncClient> client) mutable
                {
                    reportResult(std::move(handler), std::move(client));
                });

            auto url = url::Builder(m_baseApiUrl).appendPath(
                http::rest::substituteParameters(kLogSessionFragmentsPath, {sessionId}));

            ctx.executor->doPost(
                url,
                std::make_unique<http::BufferSource>("text/plain", std::move(text)),
                [this, &ctx]() { m_httpClientPool.completeRequest(&ctx); });
        });
}

void CollectorApiClient::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();
    m_httpClientPool.pleaseStopSync();
}

void CollectorApiClient::reportResult(
    nx::utils::MoveOnlyFunc<void(Result)> handler,
    std::unique_ptr<http::AsyncClient> client)
{
    if (client->failed())
    {
        return handler({
            http::StatusCode::undefined,
            SystemError::toString(client->lastSysErrorCode())});
    }

    handler({
        (http::StatusCode::Value) client->response()->statusLine.statusCode,
        client->response()->statusLine.reasonPhrase});
}

} // namespace nx::network::maintenance::log
