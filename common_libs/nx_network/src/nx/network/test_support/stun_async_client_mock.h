#ifndef STUN_ASYNC_CLIENT_MOCK_H
#define STUN_ASYNC_CLIENT_MOCK_H

#include <gmock.h>
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
    MOCK_METHOD2(sendRequest, void(stun::Message, RequestHandler));
    MOCK_CONST_METHOD0(localAddress, SocketAddress());
    MOCK_CONST_METHOD0(remoteAddress, SocketAddress());
    MOCK_METHOD1(closeConnection, void(SystemError::ErrorCode));

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

private:
    bool setIndicationHandlerImpl(int type, IndicationHandler handler)
    {
        QnMutexLocker lock(&m_mutex);
        return m_indicationHandlers.emplace(type, std::move(handler)).second;
    }

    mutable QnMutex m_mutex;
    std::map<int, IndicationHandler> m_indicationHandlers;
};

} // test
} // network
} // nx

#endif // STUN_ASYNC_CLIENT_MOCK_H
