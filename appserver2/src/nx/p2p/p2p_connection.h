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

namespace ec2 {
class QnAbstractTransaction;
}

namespace nx {
namespace p2p {

using namespace ec2;
class Connection;
//using P2pConnectionPtr = QSharedPointer<P2pConnection>;
using P2pConnectionPtr = QnSharedResourcePointer<Connection>;
using SendCounters = std::array<std::atomic<qint64>, (int(MessageType::counter))>;


class Connection:
    public QObject,
    public QnCommonModuleAware,
    public QnFromThisToShared<Connection>
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

    Connection(
        QnCommonModule* commonModule,
        const QnUuid& remoteId,
        const ApiPeerDataEx& localPeer,
        ConnectionLockGuard connectionLockGuard,
        const QUrl& remotePeerUrl,
        std::unique_ptr<QObject> opaqueObject);
    Connection(
        QnCommonModule* commonModule,
        const ApiPeerDataEx& remotePeer,
        const ApiPeerDataEx& localPeer,
        ConnectionLockGuard connectionLockGuard,
        const nx::network::WebSocketPtr& webSocket,
        std::unique_ptr<QObject> opaqueObject);
    virtual ~Connection();

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

    QObject* opaqueObject();

    const Qn::UserAccessData& getUserAccessData() const { return m_userAccessData; }
signals:
    void gotMessage(QWeakPointer<Connection> connection, nx::p2p::MessageType messageType, const QByteArray& payload);
    void stateChanged(QWeakPointer<Connection> connection, Connection::State state);
    void allDataSent(QWeakPointer<Connection> connection);
private:
    void cancelConnecting(const QString& reason);

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
    std::atomic<State> m_state{State::Connecting};

    Direction m_direction;
    QUrl m_remotePeerUrl;
    const Qn::UserAccessData m_userAccessData = Qn::kSystemAccess;

    std::unique_ptr<ConnectionLockGuard> m_connectionLockGuard;

    static SendCounters m_sendCounters;

    network::aio::Timer m_timer;
    std::unique_ptr<QObject> m_opaqueObject;
};

const char* toString(Connection::State value);

} // namespace p2p
} // namespace nx

Q_DECLARE_METATYPE(nx::p2p::P2pConnectionPtr)
Q_DECLARE_METATYPE(nx::p2p::Connection::State)
