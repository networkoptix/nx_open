#include "http_client.h"

#include <stdexcept>

#include <nx/network/http/buffer_source.h>

#include "exception.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::sdk;
using namespace nx::utils;
using namespace nx::network;

namespace {
    auto makeResolver(
        http::AsyncClient* nested, cf::promise<http::Response> promise)
    {
        return [nested, promise = std::move(promise)]() mutable
        {
            try
            {
                if (nested->failed())
                    throw std::system_error(
                        nested->lastSysErrorCode(), std::system_category(),
                        "TCP connection failed");

                auto response = *nested->response();
                response.messageBody = nested->fetchMessageBodyBuffer();

                promise.set_value(std::move(response));
            }
            catch (...)
            {
                promise.set_exception(std::current_exception());
            }
        };
    }
}

HttpClient::HttpClient()
{
}

HttpClient::~HttpClient()
{
    pleaseStopSync();
}

const nx::network::http::Request& HttpClient::request()
{
    return nested().request();
}

void HttpClient::setAdditionalRequestHeaders(http::HttpHeaders headers)
{
    nested().setAdditionalHeaders(std::move(headers));
}

void HttpClient::addAdditionalRequestHeader(
    const http::StringType& name, const http::StringType& value)
{
    nested().addAdditionalHeader(name, value);
}

void HttpClient::addAdditionalRequestHeaders(const http::HttpHeaders& headers)
{
    nested().addRequestHeaders(headers);
}

void HttpClient::removeAdditionalRequestHeader(const http::StringType& name)
{
    nested().removeAdditionalHeader(name);
}

void HttpClient::setRequestBody(std::unique_ptr<http::AbstractMsgBodySource> body)
{
    nested().setRequestBody(std::move(body));
}

cf::future<http::Response> HttpClient::request(http::Method::ValueType method, Url url)
{
    cf::promise<http::Response> promise;
    auto future = treatBrokenPromiseAsCancelled(promise.get_future());

    nested().doRequest(method, url, makeResolver(&nested(), std::move(promise)));

    return future.then(
        [method = std::move(method), url = std::move(url)](auto future) {
            try
            {
                return future.get();
            }
            catch (...)
            {
                std::throw_with_nested(Exception(
                    "HTTP " + method.toStdString() + " " + url.toStdString() + " failed",
                    ErrorCode::networkError));
            }
        });
}

cf::future<http::Response> HttpClient::get(Url url)
{
    return request(http::Method::get, std::move(url));
}

cf::future<http::Response> HttpClient::post(Url url)
{
    return request(http::Method::post, std::move(url));
}

void HttpClient::cancel()
{
    m_nested.reset();
}

std::unique_ptr<AbstractStreamSocket> HttpClient::takeSocket()
{
    if (m_nested)
        return m_nested->takeSocket();

    return nullptr;
}

void HttpClient::stopWhileInAioThread()
{
    BasicPollable::stopWhileInAioThread();
    if (m_nested)
    {
        m_nested->pleaseStopSync();
        m_nested.reset();
    }
}

http::AsyncClient& HttpClient::nested()
{
    if (!m_nested)
        m_nested.emplace();

    return *m_nested;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
