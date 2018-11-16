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
    P2pServerTransport(
        DataFormat dataFormat,
        bool websocketMode,
        std::unique_ptr<AbstractStreamSocket> socket);

    void setAliveTimeout(std::chrono::milliseconds timeout);

    bool setSecondaryOutgoingConnection(
        const vms::api::PeerDataEx& remotePeer,
        std::unique_ptr<AbstractStreamSocket> socket)
};

class P2pClientTransport: public P2pTransport
{
    p2pTransport(TransportMode mode, std::unique_ptr<AbstractStreamSocket> socket);

    virtual void connectAsync(
        const nx::utils::Url& url,
        Callback callback) override;

};

} // namespace nx::network
