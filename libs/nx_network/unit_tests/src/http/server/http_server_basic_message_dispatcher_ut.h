// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gtest/gtest.h>

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/utils/std/cpp14.h>

namespace nx::network::http::server::test {

namespace detail {

struct RequestContext
{
    std::string pathTemplate;
    RequestPathParams requestPathParams;
};

class DummyHandler:
    public nx::network::http::RequestHandlerWithContext
{
public:
    DummyHandler(
        const std::string& pathTemplate,
        std::deque<RequestContext>* requests)
        :
        m_pathTemplate(pathTemplate),
        m_requests(requests)
    {
    }

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override
    {
        m_requests->push_back({
            m_pathTemplate,
            std::exchange(requestContext.requestPathParams, {})});

        completionHandler(StatusCode::ok);
    }

private:
    std::string m_pathTemplate;
    std::deque<RequestContext>* m_requests;
};

using OnRequestProcessedHandler = std::function<void(
    nx::network::http::Message message,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> bodySource,
    ConnectionEvents connectionEvents)>;

} // namespace detail

template<typename MessageDispatcherType>
class HttpServerBasicMessageDispatcher:
    public ::testing::Test
{
public:
    ~HttpServerBasicMessageDispatcher()
    {
        m_timer.pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        m_messageDispatcher = std::make_unique<MessageDispatcherType>();
    }

    void registerHandler(const std::string& path, const std::string_view& method = kAnyMethod)
    {
        m_messageDispatcher->registerRequestProcessor(
            path,
            std::bind(&HttpServerBasicMessageDispatcher::handlerFactoryFunc, this, path),
            method);
    }

    void registerDefaultHandler()
    {
        m_messageDispatcher->registerRequestProcessor(
            nx::network::http::kAnyPath,
            std::bind(&HttpServerBasicMessageDispatcher::handlerFactoryFunc, this, "default"));
    }

    void registerSlowHandler(const std::string& path)
    {
        m_messageDispatcher->registerRequestProcessorFunc(
            kAnyMethod,
            path,
            [this](auto&&... args) { slowHandlerFunc(std::forward<decltype(args)>(args)...); });
    }

    void invokeHandler(const std::string& path, const Method& method)
    {
        assertRequestIsDispatched(path, method);
    }

    void destroyDispatcher()
    {
        m_messageDispatcher->waitUntilAllRequestsCompleted();
        m_messageDispatcher.reset();
    }

    void assertNoHandlerIsRunning()
    {
        ASSERT_EQ(0, m_runningHandlerCounter);
    }

    void assertHandlerIsFound(
        const std::string& path,
        const Method& method = nx::network::http::Method::get)
    {
        assertRequestIsDispatched(path, method);

        ASSERT_EQ(path, issuedRequest().pathTemplate);
    }

    void assertHandlerNotFound(
        const std::string& path,
        const Method& method = nx::network::http::Method::get)
    {
        ASSERT_FALSE(
            m_messageDispatcher->dispatchRequest(
                RequestContext(
                    {},
                    {},
                    SocketAddress(),
                    {},
                    prepareDummyRequest(method, path)),
                [](auto&&...) {}));
    }

    void assertDispatchFailureIsRecorded(int count)
    {
        const auto statusCodes = m_messageDispatcher->statusCodesReported();
        ASSERT_EQ(count, statusCodes.at(StatusCode::notFound));
    }

    void assertDefaultHandlerFound(
        const std::string& path,
        const Method& method = nx::network::http::Method::get)
    {
        assertRequestIsDispatched(path, method);

        ASSERT_EQ("default", issuedRequest().pathTemplate);
    }

    void assertRequestIsDispatched(const std::string& path, const Method& method)
    {
        ASSERT_TRUE(
            m_messageDispatcher->dispatchRequest(
                RequestContext(
                    ConnectionAttrs{},
                    std::weak_ptr<http::HttpServerConnection>{},
                    SocketAddress(),
                    {},
                    prepareDummyRequest(method, path)),
                [](auto&&...) {}));
    }

    detail::RequestContext issuedRequest()
    {
        auto requestContext = std::move(m_dispatchedPathQueue.front());
        m_dispatchedPathQueue.pop_front();
        return requestContext;
    }

private:
    std::unique_ptr<MessageDispatcherType> m_messageDispatcher;
    std::deque<detail::RequestContext> m_dispatchedPathQueue;
    std::atomic<int> m_runningHandlerCounter = 0;
    aio::Timer m_timer;

    std::unique_ptr<detail::DummyHandler> handlerFactoryFunc(
        const std::string& path)
    {
        return std::make_unique<detail::DummyHandler>(
            path, &m_dispatchedPathQueue);
    }

    void slowHandlerFunc(
        RequestContext /*requestContext*/,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        ++m_runningHandlerCounter;

        m_timer.start(
            std::chrono::milliseconds(100),
            [this, handler = std::move(completionHandler)]()
            {
                --m_runningHandlerCounter;
                handler(StatusCode::ok);
            });
    }

    nx::network::http::Request prepareDummyRequest(const Method& method, const std::string& path)
    {
        nx::network::http::Request request;
        request.requestLine.method = method;
        request.requestLine.version = nx::network::http::http_1_1;
        request.requestLine.url =
            nx::utils::Url(nx::utils::buildString("http://127.0.0.1:7001", path));
        return request;
    }
};

TYPED_TEST_SUITE_P(HttpServerBasicMessageDispatcher);

TYPED_TEST_P(HttpServerBasicMessageDispatcher, handler_found_by_exatch_match)
{
    this->registerHandler("/accounts/");
    this->registerHandler("/systems/");
    this->assertHandlerIsFound("/accounts/");
}

TYPED_TEST_P(HttpServerBasicMessageDispatcher, handler_not_found)
{
    this->registerHandler("/accounts/");
    this->registerHandler("/systems/");

    this->assertHandlerNotFound("/accounts/accountId/systems");
    this->assertHandlerNotFound("/users/");
    this->assertHandlerNotFound("/accounts");
    this->assertHandlerNotFound("/");
    this->assertHandlerNotFound("");
}

TYPED_TEST_P(HttpServerBasicMessageDispatcher, records_dispatch_failures)
{
    this->assertHandlerNotFound("/accounts/accountId/systems");
    this->assertHandlerNotFound("/users/");
    this->assertHandlerNotFound("/accounts");
    this->assertHandlerNotFound("/");
    this->assertHandlerNotFound("");

    this->assertDispatchFailureIsRecorded(5);
}

TYPED_TEST_P(HttpServerBasicMessageDispatcher, default_handler_is_used)
{
    this->registerDefaultHandler();

    this->assertDefaultHandlerFound("/accounts/accountId/systems");
    this->assertDefaultHandlerFound("/users");
    this->assertDefaultHandlerFound("/");
}

TYPED_TEST_P(HttpServerBasicMessageDispatcher, default_handler_has_lowest_priority)
{
    this->registerHandler("/accounts/");
    this->registerDefaultHandler();

    this->assertHandlerIsFound("/accounts/");
    this->assertDefaultHandlerFound("/accounts/accountId/systems");
    this->assertDefaultHandlerFound("/users");
    this->assertDefaultHandlerFound("/");
}

TYPED_TEST_P(HttpServerBasicMessageDispatcher, register_handler_for_specific_method)
{
    this->registerHandler("/accounts/", nx::network::http::Method::get);

    this->assertHandlerIsFound("/accounts/", nx::network::http::Method::get);
    this->assertHandlerNotFound("/accounts/", nx::network::http::Method::post);
}

TYPED_TEST_P(HttpServerBasicMessageDispatcher, waits_for_handler_completion_before_destruction)
{
    this->registerSlowHandler("/slow");

    this->invokeHandler("/slow", nx::network::http::Method::get);
    this->destroyDispatcher();

    this->assertNoHandlerIsRunning();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(HttpServerBasicMessageDispatcher);
REGISTER_TYPED_TEST_SUITE_P(HttpServerBasicMessageDispatcher,
    handler_found_by_exatch_match,
    handler_not_found, records_dispatch_failures, default_handler_is_used,
    default_handler_has_lowest_priority,
    register_handler_for_specific_method,
    waits_for_handler_completion_before_destruction);

} // namespace nx::network::http::server::test
