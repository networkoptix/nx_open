#pragma once

#include <nx_ec/data/api_peer_data.h>
#include "p2p_fwd.h"

#include <nx/network/websocket/websocket.h>
#include <nx/network/http/asynchttpclient.h>
#include <common/common_module_aware.h>
#include <nx_ec/ec_proto_version.h>

namespace ec2 {

class P2pConnection:
    public QObject,
    public QnCommonModuleAware
{
public:

    enum class State
    {
        Connecting,
        Connected,
        Error
    };

    enum class Direction
    {
        incoming,
        outgoing,
    };

    P2pConnection(
        QnCommonModule* commonModule,
        const QnUuid& remoteId,
        const ApiPeerData& localPeer,
        const QUrl& url);
    P2pConnection(
        QnCommonModule* commonModule,
        const ApiPeerData& remotePeer,
        const ApiPeerData& localPeer,
        const WebSocketPtr& webSocket);

    /** Peer that opens this connection */
    Direction direction() const;

    ApiPeerData localPeer() const;
    ApiPeerData remotePeer() const;

    void unsubscribeFrom(const ApiPersistentIdData& idList);
    void subscribeTo(const std::vector<ApiPersistentIdData>& idList);

    State state() const;
    void sendMessage(const nx::Buffer& data);

    qint64 remoteIdentityTime() const;
private:
    void cancelConnecting();

    void onResponseReceived(const nx_http::AsyncHttpClientPtr& client);
    void onHttpClientDone(const nx_http::AsyncHttpClientPtr& httpClient);

    void setState(State state);
    void fillAuthInfo(const nx_http::AsyncHttpClientPtr& httpClient, bool authByKey);
    void setRemoteIdentityTime(qint64 time);

    void onMessageSent(SystemError::ErrorCode errorCode, size_t bytesSent);
    void onNewMessageRead(SystemError::ErrorCode errorCode, size_t bytesRead);
private:
    enum class CredentialsSource
    {
        remoteUrl,
        serverKey,
        appserverConnectionFactory,
        none,
    };
private:
    mutable QnMutex m_mutex;

    std::deque<nx::Buffer> m_dataToSend;
    QByteArray m_readBuffer;

    nx_http::AsyncHttpClientPtr m_httpClient;
    WebSocketPtr m_webSocket;

    State m_state = State::Connecting;
    CredentialsSource m_credentialsSource = CredentialsSource::serverKey;

    ApiPeerData m_localPeer;
    ApiPeerData m_remotePeer;
    Direction m_direction;

    qint64 m_remoteIdentityTime = 0;
    int m_remotePeerEcProtoVersion = nx_ec::INITIAL_EC2_PROTO_VERSION;
    int m_localPeerProtocolVersion = nx_ec::EC2_PROTO_VERSION;

};
using P2pConnectionPtr = std::shared_ptr<P2pConnection>;

} // namespace ec2
