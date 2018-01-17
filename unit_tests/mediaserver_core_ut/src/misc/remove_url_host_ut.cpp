#include <gtest/gtest.h>

#include <nx/network/http/http_types.h>
#include <proxy/proxy_connection.h>

TEST(CleanupProxyInfo, urlWithoutAuth)
{
    nx::network::http::Request request;
    request.requestLine.url = nx::utils::Url("http://localhost:7013/path1?param1=123#fragment");
    QnProxyConnectionProcessor::cleanupProxyInfo(&request);
    auto updatedUrl = request.requestLine.url.toString();
    ASSERT_EQ("/path1?param1=123#fragment", updatedUrl);
}

TEST(CleanupProxyInfo, urlWithAuth)
{
    nx::network::http::Request request;
    request.requestLine.url = nx::utils::Url("http://admin:admin@localhost:7013/path1?param1=123#fragment");
    QnProxyConnectionProcessor::cleanupProxyInfo(&request);
    auto updatedUrl = request.requestLine.url.toString();
    ASSERT_EQ("/path1?param1=123#fragment", updatedUrl);
}

TEST(CleanupProxyInfo, proxyHeaders)
{
    nx::network::http::Request request;
    request.headers.emplace("Proxy-", "value1");
    request.headers.emplace("Proxy-Authorization", "value2");
    request.headers.emplace("NonProxy-Authorization", "value3");
    QnProxyConnectionProcessor::cleanupProxyInfo(&request);
    ASSERT_EQ(1, request.headers.size());
}
