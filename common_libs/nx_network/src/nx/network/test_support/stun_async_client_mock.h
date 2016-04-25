#ifndef STUN_ASYNC_CLIENT_MOCK_H
#define STUN_ASYNC_CLIENT_MOCK_H

#include <gmock/gmock.h>
#include <nx/network/stun/async_client.h>

namespace nx {
namespace network {
namespace test {

class StunAsyncClientMock: public stun::AbstractAsyncClient
{
public:
    StunAsyncClientMock()
    {
        ON_CALL(*this, setIndicationHandler(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                this, &StunAsyncClientMock::setIndicationHandlerImpl));
        ON_CALL(*this, localAddress())
            .WillByDefault(::testing::Return(SocketAddress()));
        ON_CALL(*this, remoteAddress())
            .WillByDefault(::testing::Return(SocketAddress()));
    }

    MOCK_METHOD2(connect, void(SocketAddress, bool));
    MOCK_METHOD2(setIndicationHandler, bool(int, IndicationHandler));
    MOCK_METHOD1(ignoreIndications, bool(int));
    MOCK_CONST_METHOD0(localAddress, SocketAddress());
    MOCK_CONST_METHOD0(remoteAddress, SocketAddress());
    MOCK_METHOD1(closeConnection, void(SystemError::ErrorCode));

    void sendRequest(stun::Message request, RequestHandler handler) override
    {
        QnMutexLocker lock(&m_mutex);
        const auto it = m_requestHandlers.find(request.header.method);
        ASSERT_EQ(request.header.messageClass, stun::MessageClass::request);
        ASSERT_NE(it, m_requestHandlers.end());

        const auto requestHandler = it->second; // copy
        lock.unlock();
        requestHandler(std::move(request), std::move(handler));
    }

    void emulateIndication(stun::Message indication)
    {
        IndicationHandler handler;
        {
            QnMutexLocker lock(&m_mutex);
            const auto it = m_indicationHandlers.find(indication.header.method);
            ASSERT_NE(it, m_indicationHandlers.end());
            handler = it->second;
        }

        handler(std::move(indication));
    }

    typedef std::function<void(stun::Message, RequestHandler)> ServerRequestHandler;
    void emulateRequestHandler(int message, ServerRequestHandler handler)
    {
        QnMutexLocker lock(&m_mutex);
        m_requestHandlers.emplace(std::move(message), std::move(handler));
    }

private:
    bool setIndicationHandlerImpl(int type, IndicationHandler handler)
    {
        QnMutexLocker lock(&m_mutex);
        return m_indicationHandlers.emplace(type, std::move(handler)).second;
    }

    mutable QnMutex m_mutex;
    std::map<int, ServerRequestHandler> m_requestHandlers;
    std::map<int, IndicationHandler> m_indicationHandlers;
};

} // test
} // network
} // nx

#endif // STUN_ASYNC_CLIENT_MOCK_H
