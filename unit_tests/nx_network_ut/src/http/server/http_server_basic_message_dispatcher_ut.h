#pragma once

#include <gtest/gtest.h>

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/utils/std/cpp14.h>

namespace nx_http {
namespace server {
namespace test {

namespace {

class DummyHandler:
    public nx_http::AbstractHttpRequestHandler
{
public:
    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler /*completionHandler*/) override
    {
    }
};

using OnRequestProcessedHandler = std::function<void(
    nx_http::Message message,
    std::unique_ptr<nx_http::AbstractMsgBodySource> bodySource,
    ConnectionEvents connectionEvents)>;

} // namespace

template<typename MessageDispatcherType>
class HttpServerBasicMessageDispatcher:
    public ::testing::Test
{
protected:
    void registerHandler(const std::string& path, nx_http::StringType method = kAnyMethod)
    {
        m_messageDispatcher.registerRequestProcessor<DummyHandler>(
            QString::fromStdString(path),
            std::bind(&HttpServerBasicMessageDispatcher::handlerFactoryFunc, this, path),
            method);
    }

    void registerDefaultHandler()
    {
        m_messageDispatcher.registerRequestProcessor<DummyHandler>(
            nx_http::kAnyPath,
            std::bind(&HttpServerBasicMessageDispatcher::handlerFactoryFunc, this, "default"));
    }

    void assertHandlerIsFound(
        const std::string& path,
        nx_http::StringType method = nx_http::Method::GET)
    {
        ASSERT_TRUE(
            m_messageDispatcher.dispatchRequest(
                nullptr,
                prepareDummyMessage(method, path),
                nx::utils::stree::ResourceContainer(),
                OnRequestProcessedHandler()));
        ASSERT_EQ(path, m_dispatchedPathQueue.front());
        m_dispatchedPathQueue.pop_front();
    }

    void assertHandlerNotFound(
        const std::string& path,
        nx_http::StringType method = nx_http::Method::GET)
    {
        ASSERT_FALSE(
            m_messageDispatcher.dispatchRequest(
                nullptr,
                prepareDummyMessage(method, path),
                nx::utils::stree::ResourceContainer(),
                OnRequestProcessedHandler()));
    }

    void assertDefaultHandlerFound(
        const std::string& path,
        nx_http::StringType method = nx_http::Method::GET)
    {
        ASSERT_TRUE(
            m_messageDispatcher.dispatchRequest(
                nullptr,
                prepareDummyMessage(method, path),
                nx::utils::stree::ResourceContainer(),
                OnRequestProcessedHandler()));
        ASSERT_EQ("default", m_dispatchedPathQueue.front());
        m_dispatchedPathQueue.pop_front();
    }

private:
    MessageDispatcherType m_messageDispatcher;
    std::deque<std::string> m_dispatchedPathQueue;

    std::unique_ptr<DummyHandler> handlerFactoryFunc(const std::string& path)
    {
        m_dispatchedPathQueue.push_back(path);
        return std::make_unique<DummyHandler>();
    }

    nx_http::Message prepareDummyMessage(
        const nx_http::StringType& method,
        const std::string& path)
    {
        nx_http::Message message(nx_http::MessageType::request);
        message.request->requestLine.method = method;
        message.request->requestLine.version = nx_http::http_1_1;
        message.request->requestLine.url =
            QUrl(QString("http://127.0.0.1:7001%1").arg(path.c_str()));
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
    this->registerHandler("/accounts/", nx_http::Method::GET);
    
    this->assertHandlerIsFound("/accounts/", nx_http::Method::GET);
    this->assertHandlerNotFound("/accounts/", nx_http::Method::POST);
}

REGISTER_TYPED_TEST_CASE_P(
    HttpServerBasicMessageDispatcher,
    handler_found_by_exatch_match, handler_not_found, default_handler_is_used, 
    default_handler_has_lowest_priority, register_handler_for_specific_method);

} // namespace test
} // namespace server
} // namespace nx_http
