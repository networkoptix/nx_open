#pragma once

#include <QtCore/QElapsedTimer>

#include "p2p_fwd.h"
#include <nx_ec/data/api_peer_data.h>
#include <nx/network/websocket/websocket.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/common/from_this_to_shared.h>
#include <core/resource/shared_resource_pointer.h>
#include <core/resource_access/user_access_data.h>
#include <transaction/connection_guard.h>
#include <nx_ec/data/api_tran_state_data.h>
#include <nx/network/http/http_async_client.h>
#include <transaction/abstract_transaction_transport.h>

namespace ec2 {
class QnAbstractTransaction;
}

namespace nx {
namespace p2p {

using namespace ec2;
class ConnectionBase;
//using P2pConnectionPtr = QSharedPointer<P2pConnection>;
using P2pConnectionPtr = QnSharedResourcePointer<ConnectionBase>;
using SendCounters = std::array<std::atomic<qint64>, (int(MessageType::counter))>;


class ConnectionBase:
    public QnAbstractTransactionTransport,
    public QnFromThisToShared<ConnectionBase>
{
    Q_OBJECT
public:

    enum class State
    {
        Connecting,
        Connected,
        Error,
        Unauthorized
    };

    enum class Direction
    {
        incoming,
        outgoing,
    };

    ConnectionBase(
        const QnUuid& remoteId,
        const ApiPeerDataEx& localPeer,
        ConnectionLockGuard connectionLockGuard,
        const QUrl& remotePeerUrl,
        const std::chrono::seconds& keepAliveTimeout,
        std::unique_ptr<QObject> opaqueObject);
    ConnectionBase(
        const ApiPeerDataEx& remotePeer,
        const ApiPeerDataEx& localPeer,
        ConnectionLockGuard connectionLockGuard,
        nx::network::WebSocketPtr webSocket,
        const Qn::UserAccessData& userAccessData,
        std::unique_ptr<QObject> opaqueObject);
    virtual ~ConnectionBase();

    static const SendCounters& sendCounters() { return m_sendCounters;  }

    /** Peer that opens this connection */
    Direction direction() const { return m_direction; }

    virtual const ApiPeerData& localPeer() const override { return m_localPeer; }
    virtual const ApiPeerData& remotePeer() const override { return m_remotePeer; }
    virtual bool isIncoming() const override { return m_direction == Direction::incoming;  }
    virtual nx_http::AuthInfoCache::AuthorizationCacheItem authData() const override;

    State state() const;
    void setState(State state);

    void sendMessage(MessageType messageType, const nx::Buffer& data);
    void sendMessage(const nx::Buffer& data);

    void startConnection();
    void startReading();

    QObject* opaqueObject();

    const Qn::UserAccessData& userAccessData() const { return m_userAccessData; }
    virtual QUrl remoteAddr() const override;
signals:
    void gotMessage(QWeakPointer<ConnectionBase> connection, nx::p2p::MessageType messageType, const QByteArray& payload);
    void stateChanged(QWeakPointer<ConnectionBase> connection, ConnectionBase::State state);
    void allDataSent(QWeakPointer<ConnectionBase> connection);
protected:
    virtual void fillAuthInfo(nx_http::AsyncClient* httpClient, bool authByKey) = 0;
private:
    void cancelConnecting(State state, const QString& reason);

    void onHttpClientDone();

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
    std::chrono::seconds m_keepAliveTimeout;
    std::unique_ptr<QObject> m_opaqueObject;

    int m_sendSequence = 0;
    int m_lastReceivedSequence = 0;

    nx_http::AuthInfoCache::AuthorizationCacheItem m_httpAuthCacheItem;
    mutable QnMutex m_mutex;
};

QString toString(ConnectionBase::State value);

} // namespace p2p
} // namespace nx

Q_DECLARE_METATYPE(nx::p2p::P2pConnectionPtr)
Q_DECLARE_METATYPE(nx::p2p::ConnectionBase::State)
