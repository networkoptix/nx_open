#pragma once

#include <QtCore/QString>
#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>
#include <nx/network/buffer.h>
#include <nx/network/abstract_socket.h>
#include <nx/utils/url.h>
#include <nx/network/aio/basic_pollable.h>

namespace nx::network {

using IoCompletionHandler = nx::utils::MoveOnlyFunc<void(
    SystemError::ErrorCode /*errorCode*/, std::size_t /*bytesTransferred*/)>;

using PostConnectionOpenedCallback = nx::utils::MoveOnlyFunc<void()>;

class P2pTransport: public aio::BasicPollable
{
public:


    enum class DataFormat
    {
        text,
        binary
    };

    SocketAddress getForeignAddress() const;

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler);
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler);
};

class P2pWebsocketServerTransport: public P2pTransport
{
public:
    P2pWebsocketServerTransport(
        DataFormat dataFormat,
        std::unique_ptr<AbstractStreamSocket> socket);
};

class P2pHttpServerTransport: public P2pTransport
{
public:
    P2pHttpServerTransport(
        DataFormat dataFormat,
        std::unique_ptr<AbstractStreamSocket> getSocket);

    void gotPostConnection(
        std::unique_ptr<AbstractStreamSocket> postSocket);
};

class P2pWebsocketClientTransport: public P2pTransport
{
public:
    P2pWebsocketClientTransport(
        DataFormat dataFormat,
        std::unique_ptr<AbstractStreamSocket> socket);
};

class P2pHttpClientTransport : public P2pTransport
{
public:
    P2pHttpClientTransport(
        DataFormat dataFormat,
        std::unique_ptr<AbstractStreamSocket> socket,
        const nx::utils::Url& postConnectionUrl,
        PostConnectionOpenedCallback callback);

};

using P2pTransportPtr = std::unique_ptr<P2pTransport>;

} // namespace nx::network
