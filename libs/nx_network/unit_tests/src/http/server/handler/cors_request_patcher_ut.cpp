// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <future>

#include <gtest/gtest.h>

#include <nx/network/http/server/handler/cors_request_patcher.h>

namespace nx::network::http::server::handler::test {

namespace {

class TestHandler:
    public AbstractRequestHandler
{
public:
    virtual void serve(
        RequestContext /*requestContext*/,
        nx::utils::MoveOnlyFunc<void(RequestResult)> completionHandler) override
    {
        RequestResult result(StatusCode::ok);
        result.headers.emplace("TestHandler", "test");
        completionHandler(std::move(result));
    }
};

} // namespace

class CorsRequestPatcher:
    public ::testing::Test
{
public:
    CorsRequestPatcher():
        m_patcher(&m_testHandler)
    {
    }

protected:
    CorsConfiguration registerRandomCorsConfForOrigin(const std::string& originMask)
    {
        CorsConfiguration conf;
        conf.allowHeaders = nx::utils::generateRandomName(12);
        m_patcher.addAllowedOrigin(originMask, conf);
        return conf;
    }

    void whenReceiveRequestFrom(Method method, const std::string& origin)
    {
        RequestContext ctx;
        ctx.request.requestLine = RequestLine{.method = method, .url = "/test", .version = http_1_1};
        if (!origin.empty())
        {
            ctx.request.headers.emplace("Origin", origin);
            m_lastUsedOrigin = origin;
        }

        std::promise<void> done;
        m_patcher.serve(
            std::move(ctx),
            [this, &done](RequestResult response)
            {
                m_lastResponse = std::move(response);
                done.set_value();
            });
        done.get_future().wait();
    }

    void thenOriginIsMatched(const CorsConfiguration& expectedConf)
    {
        assertHeaderEqual(expectedConf.allowHeaders, "Access-Control-Allow-Headers");
        assertHeaderEqual(m_lastUsedOrigin, "Access-Control-Allow-Origin");
    }

    void thenRequestIsForwarded()
    {
        ASSERT_TRUE(m_lastResponse->headers.contains("TestHandler"));
    }

private:
    void assertHeaderEqual(const std::string& expected, const std::string headerName)
    {
        auto it = m_lastResponse->headers.find(headerName);
        ASSERT_NE(m_lastResponse->headers.end(), it);
        ASSERT_EQ(expected, it->second);
    }

private:
    TestHandler m_testHandler;
    handler::CorsRequestPatcher m_patcher;
    std::optional<RequestResult> m_lastResponse;
    std::string m_lastUsedOrigin;
};

TEST_F(CorsRequestPatcher, options_is_implemented)
{
    const auto conf = registerRandomCorsConfForOrigin("*");
    whenReceiveRequestFrom(Method::options, "https://example.com");
    thenOriginIsMatched(conf);
}

TEST_F(CorsRequestPatcher, most_specific_origin_is_chosen)
{
    const auto conf1 = registerRandomCorsConfForOrigin("*");
    const auto conf2 = registerRandomCorsConfForOrigin("*://example.com*");

    whenReceiveRequestFrom(Method::get, "https://example.com");
    thenOriginIsMatched(conf2);

    whenReceiveRequestFrom(Method::get, "https://foo.com");
    thenOriginIsMatched(conf1);
}

TEST_F(CorsRequestPatcher, request_without_origin_is_forwarded)
{
    const auto conf1 = registerRandomCorsConfForOrigin("*");

    whenReceiveRequestFrom(Method::get, "");
    thenRequestIsForwarded();
}

} // namespace nx::network::http::server::handler::test
