// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <gmock/gmock.h>
#include <nx/network/stun/async_client.h>

namespace nx {
namespace network {
namespace stun {
namespace test {

class AsyncClientMock:
    public AbstractAsyncClient
{
public:
    AsyncClientMock()
    {
        ON_CALL(*this, setIndicationHandler(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                this, &AsyncClientMock::setIndicationHandlerImpl));
        ON_CALL(*this, localAddress())
            .WillByDefault(::testing::Return(SocketAddress()));
        ON_CALL(*this, remoteAddress())
            .WillByDefault(::testing::Return(SocketAddress()));
    }

    virtual ~AsyncClientMock() override
    {
        stopWhileInAioThread();
    }

    virtual void connect(const nx::utils::Url&, ConnectHandler) override {}

    virtual void addOnReconnectedHandler(
        ReconnectHandler /*handler*/, void* /*client*/ = 0) override {}

    MOCK_METHOD(void, setIndicationHandler, (int, IndicationHandler, void*));
    MOCK_METHOD(bool, addConnectionTimer, (std::chrono::milliseconds, TimerHandler, void*));
    MOCK_METHOD(SocketAddress, localAddress, (), (const));
    MOCK_METHOD(SocketAddress, remoteAddress, (), (const));
    MOCK_METHOD(void, closeConnection, (SystemError::ErrorCode));
    MOCK_METHOD(void, setKeepAliveOptions, (KeepAliveOptions));

    virtual void setOnConnectionClosedHandler(
        OnConnectionClosedHandler /*onConnectionClosedHandler*/) override
    {
        // TODO
    }

    virtual void sendRequest(Message request, RequestHandler handler, void*) override
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        const auto it = m_requestHandlers.find(request.header.method);
        ASSERT_EQ(request.header.messageClass, stun::MessageClass::request);
        ASSERT_NE(it, m_requestHandlers.end());

        const auto requestHandler = it->second; // copy
        lock.unlock();
        requestHandler(std::move(request), std::move(handler));
    }

    virtual void cancelHandlers(void*, utils::MoveOnlyFunc<void()> handler) override
    {
        handler();
    }

    virtual void cancelHandlersSync(void*) override
    {
    }

    void emulateIndication(stun::Message indication)
    {
        IndicationHandler handler;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            const auto it = m_indicationHandlers.find(indication.header.method);
            ASSERT_NE(it, m_indicationHandlers.end());
            handler = it->second;
        }

        handler(std::move(indication));
    }

    typedef std::function<void(stun::Message, RequestHandler)> ServerRequestHandler;
    void emulateRequestHandler(int message, ServerRequestHandler handler)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_requestHandlers.emplace(std::move(message), std::move(handler));
    }

private:
    bool setIndicationHandlerImpl(int type, IndicationHandler handler, void*)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        return m_indicationHandlers.emplace(type, std::move(handler)).second;
    }

    mutable nx::Mutex m_mutex;
    std::map<int, ServerRequestHandler> m_requestHandlers;
    std::map<int, IndicationHandler> m_indicationHandlers;

    virtual void stopWhileInAioThread() override
    {
    }
};

} // namespace test
} // namespace stun
} // namespace network
} // namespace nx
