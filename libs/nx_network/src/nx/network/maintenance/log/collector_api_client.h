// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/aio/async_operation_pool.h>
#include <nx/network/http/http_async_client.h>

#include "collector_api_paths.h"

namespace nx::network::maintenance::log {

struct Result
{
    http::StatusCode::Value result = http::StatusCode::undefined;
    std::string text;
};

class CollectorApiClient:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    CollectorApiClient(const nx::utils::Url& baseApiUrl);
    ~CollectorApiClient();

    virtual void bindToAioThread(aio::AbstractAioThread*) override;

    void upload(
        const std::string& sessionId,
        nx::Buffer text,
        nx::utils::MoveOnlyFunc<void(Result)> handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    void reportResult(
        nx::utils::MoveOnlyFunc<void(Result)> handler,
        std::unique_ptr<http::AsyncClient> client);

private:
    aio::AsyncOperationPool<http::AsyncClient> m_httpClientPool;
    const nx::utils::Url m_baseApiUrl;
};

} // namespace nx::network::maintenance::log
