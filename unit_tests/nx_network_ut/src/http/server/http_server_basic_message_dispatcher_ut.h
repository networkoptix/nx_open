#pragma once

#include <gtest/gtest.h>

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace http {
namespace server {
namespace test {

namespace detail {

struct RequestContext
{
    nx::network::http::StringType pathTemplate;
    std::vector<nx::network::http::StringType> requestPathParams;
};

class DummyHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    DummyHandler(
        const nx::network::http::StringType& pathTemplate,
        std::deque<RequestContext>* requests)
        :
        m_pathTemplate(pathTemplate),
        m_requests(requests)
    {
    }

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler /*completionHandler*/) override
    {
        m_requests->push_back({m_pathTemplate, requestPathParams()});
    }

private:
    nx::network::http::StringType m_pathTemplate;
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
protected:
    void registerHandler(const nx::network::http::StringType& path, nx::network::http::StringType method = kAnyMethod)
    {
        m_messageDispatcher.template registerRequestProcessor<detail::DummyHandler>(
            path,
            std::bind(&HttpServerBasicMessageDispatcher::handlerFactoryFunc, this, path),
            method);
    }

    void registerDefaultHandler()
    {
        m_messageDispatcher.template registerRequestProcessor<detail::DummyHandler>(
            nx::network::http::kAnyPath,
            std::bind(&HttpServerBasicMessageDispatcher::handlerFactoryFunc, this, "default"));
    }

    void assertHandlerIsFound(
        const nx::network::http::StringType& path,
        nx::network::http::StringType method = nx::network::http::Method::get)
    {
        assertRequestIsDispatched(path, method);

        ASSERT_EQ(path, issuedRequest().pathTemplate);
    }

    void assertHandlerNotFound(
        const nx::network::http::StringType& path,
        nx::network::http::StringType method = nx::network::http::Method::get)
    {
        ASSERT_FALSE(
            m_messageDispatcher.dispatchRequest(
                nullptr,
                prepareDummyMessage(method, path),
                nx::utils::stree::ResourceContainer(),
                detail::OnRequestProcessedHandler()));
    }

    void assertDefaultHandlerFound(
        const nx::network::http::StringType& path,
        nx::network::http::StringType method = nx::network::http::Method::get)
    {
        assertRequestIsDispatched(path, method);

        ASSERT_EQ("default", issuedRequest().pathTemplate);
    }

    void assertRequestIsDispatched(
        const nx::network::http::StringType& path,
        nx::network::http::StringType method)
    {
        ASSERT_TRUE(
            m_messageDispatcher.dispatchRequest(
                nullptr,
                prepareDummyMessage(method, path),
                nx::utils::stree::ResourceContainer(),
                detail::OnRequestProcessedHandler()));
    }

    detail::RequestContext issuedRequest()
    {
        auto requestContext = std::move(m_dispatchedPathQueue.front());
        m_dispatchedPathQueue.pop_front();
        return requestContext;
    }

private:
    MessageDispatcherType m_messageDispatcher;
    std::deque<detail::RequestContext> m_dispatchedPathQueue;

    std::unique_ptr<detail::DummyHandler> handlerFactoryFunc(
        const nx::network::http::StringType& path)
    {
        return std::make_unique<detail::DummyHandler>(
            path, &m_dispatchedPathQueue);
    }

    nx::network::http::Message prepareDummyMessage(
        const nx::network::http::StringType& method,
        const nx::network::http::StringType& path)
    {
        nx::network::http::Message message(nx::network::http::MessageType::request);
        message.request->requestLine.method = method;
        message.request->requestLine.version = nx::network::http::http_1_1;
        message.request->requestLine.url =
            nx::utils::Url(QString("http://127.0.0.1:7001%1").arg(QString::fromUtf8(path)));
        return message;
    }
};

TYPED_TEST_CASE_P(HttpServerBasicMessageDispatcher);

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

REGISTER_TYPED_TEST_CASE_P(
    HttpServerBasicMessageDispatcher,
    handler_found_by_exatch_match, handler_not_found, default_handler_is_used,
    default_handler_has_lowest_priority, register_handler_for_specific_method);

} // namespace test
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
