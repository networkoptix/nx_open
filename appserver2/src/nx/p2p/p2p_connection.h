#pragma once

#include <nx_ec/data/api_peer_data.h>
#include "p2p_fwd.h"

#include <nx/network/websocket/websocket.h>
#include <nx/network/http/asynchttpclient.h>
#include <common/common_module_aware.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/common/from_this_to_shared.h>
#include <core/resource/shared_resource_pointer.h>
#include <core/resource_access/user_access_data.h>
#include <QtCore/QElapsedTimer>
#include <transaction/connection_guard.h>
#include <nx_ec/data/api_tran_state_data.h>
#include <nx/network/http/http_async_client.h>
#include "connection_context.h"

namespace ec2 {
class QnAbstractTransaction;
}

namespace nx {
namespace p2p {

using namespace ec2;
class P2pConnection;
//using P2pConnectionPtr = QSharedPointer<P2pConnection>;
using P2pConnectionPtr = QnSharedResourcePointer<P2pConnection>;
using SendCounters = std::array<std::atomic<qint64>, (int(MessageType::counter))>;


class P2pConnection:
    public QObject,
    public QnCommonModuleAware,
    public QnFromThisToShared<P2pConnection>
{
    Q_OBJECT
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
        const ApiPeerDataEx& localPeer,
        ConnectionLockGuard connectionLockGuard,
        const QUrl& remotePeerUrl);
    P2pConnection(
        QnCommonModule* commonModule,
        const ApiPeerDataEx& remotePeer,
        const ApiPeerDataEx& localPeer,
        ConnectionLockGuard connectionLockGuard,
        const nx::network::WebSocketPtr& webSocket);
    virtual ~P2pConnection();

    static const SendCounters& sendCounters() { return m_sendCounters;  }

    /** Peer that opens this connection */
    Direction direction() const { return m_direction; }

    const ApiPeerData& localPeer() const { return m_localPeer; }
    const ApiPeerData& remotePeer() const { return m_remotePeer; }

    State state() const;
    void setState(State state);

    void sendMessage(MessageType messageType, const nx::Buffer& data);
    void sendMessage(const nx::Buffer& data);

    void startConnection();
    void startReading();

    ConnectionContext& context();

    const Qn::UserAccessData& getUserAccessData() const { return m_userAccessData; }
signals:
    void gotMessage(QWeakPointer<P2pConnection> connection, nx::p2p::MessageType messageType, const QByteArray& payload);
    void stateChanged(QWeakPointer<P2pConnection> connection, P2pConnection::State state);
    void allDataSent(QWeakPointer<P2pConnection> connection);
private:
    void cancelConnecting();

    void onHttpClientDone();

    void fillAuthInfo(nx_http::AsyncClient* httpClient, bool authByKey);

    void onMessageSent(SystemError::ErrorCode errorCode, size_t bytesSent);
    void onNewMessageRead(SystemError::ErrorCode errorCode, size_t bytesRead);

    bool handleMessage(const nx::Buffer& message);
private:
    enum class CredentialsSource
    {
        remoteUrl,
        serverKey,
        appserverConnectionFactory,
        none,
    };
private:
    std::deque<nx::Buffer> m_dataToSend;
    QByteArray m_readBuffer;

    std::unique_ptr<nx_http::AsyncClient> m_httpClient;


    CredentialsSource m_credentialsSource = CredentialsSource::serverKey;

    ApiPeerDataEx m_remotePeer;
    ApiPeerDataEx m_localPeer;

    nx::network::WebSocketPtr m_webSocket;
    std::atomic<State> m_state = State::Connecting;

    Direction m_direction;
    ConnectionContext m_context;
    QUrl m_remotePeerUrl;
    const Qn::UserAccessData m_userAccessData = Qn::kSystemAccess;

    std::unique_ptr<ConnectionLockGuard> m_connectionLockGuard;

    static SendCounters m_sendCounters;

    network::aio::Timer m_timer;
};

const char* toString(P2pConnection::State value);

} // namespace p2p
} // namespace nx

Q_DECLARE_METATYPE(nx::p2p::P2pConnectionPtr)
Q_DECLARE_METATYPE(nx::p2p::P2pConnection::State)
