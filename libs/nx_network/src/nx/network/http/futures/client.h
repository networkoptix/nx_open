// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/cf/cfuture.h>
#include "../http_async_client.h"

namespace nx::network::http::futures {

/**
 * A simple wrapper for nx::network::http::AsyncClient with future-based API.
 * 
 * If the request is failed(), std::system_error is thrown with lastSysErrorCode() through the
 * returned future.
 * If request is canceled, std::system_error with std::ercc::operation_canceled is thrown
 * through the returned future.
 * On success the entire response is returned, including Response::messageBody containing the
 * result of fetchMessageBodyBuffer() (meaning only the last part of the body if you've been
 * fetching it in chunks from in setOnSomeMessageBodyAvailable callback).
 */
class NX_NETWORK_API Client: public AsyncClient
{
public:
    using AsyncClient::AsyncClient;

    cf::future<Response> get(const nx::utils::Url& url);

    cf::future<Response> head(const nx::utils::Url& url);

    using AsyncClient::post;
    cf::future<Response> post(const nx::utils::Url& url);
    cf::future<Response> post(const nx::utils::Url& url,
        std::unique_ptr<AbstractMsgBodySource>&& requestBody);
    cf::future<Response> post(const nx::utils::Url& url,
        const std::string& contentType, nx::Buffer requestBody);

    cf::future<Response> put(const nx::utils::Url& url);
    cf::future<Response> put(const nx::utils::Url& url,
        std::unique_ptr<AbstractMsgBodySource>&& requestBody);
    cf::future<Response> put(const nx::utils::Url& url,
        const std::string& contentType, nx::Buffer requestBody);

    cf::future<Response> delete_(const nx::utils::Url& url);

    cf::future<Response> upgrade(const nx::utils::Url& url, const std::string& targetProtocol);
    cf::future<Response> upgrade(const nx::utils::Url& url,
        const Method& method, const std::string& targetProtocol);

    cf::future<Response> connect(const nx::utils::Url& url, const std::string& targetHost);

    using AsyncClient::request;
    cf::future<Response> request(Method method, const nx::utils::Url& url);
};

} // namespace nx::network::http::futures
