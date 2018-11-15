#pragma once

#include <QtCore/QString>

namespace nx::network {

class P2pTransport
{
public:

    enum class DataFormat
    {
        text,
        binary
    };

    virtual void readSomeAsync(nx::Buffer* const buffer, IoCompletionHandler handler) override;
    virtual void sendAsync(const nx::Buffer& buffer, IoCompletionHandler handler) override;
};

class P2pServerTransport: public P2pTransport
{
    enum class Direction
    {
        bidirection,
        incoming,
        outgoing
    };

    P2pServerTransport(DataFormat dataFormat);

    static QnUuid connectionGuid(nx::network::http::Request& request);

    void setAliveTimeout(std::chrono::milliseconds timeout);

    void newIncomingConnectionReceived(
        std::unique_ptr<StreamingSocket> socket,
        const QnUuid& connectionGuid,
        Direction direction);
};

class P2pClientTransport: public P2pTransport
{
    enum class TransportMode
    {
        automatic,
        websocket,
        http
    };

    p2pTransport(TransportMode mode, std::unique_ptr<StreamingSocket> socket);

    /*
     * Connect to remote host using websocket or two HTTP connections.
     * Server listen requests at different URL patches. In automatic mode if
     * websocket is fail, P2pClientTransport should use alternative url patch
     * to switch to the HTTP get and post
     */
    virtual void connectAsync(
        const nx::utils::Url& url,
        const QString& websocketPath,
        const QString& getRequestPath,
        const QString& postRequestPath,
        Callback callback) override;

};

} // namespace nx::network
