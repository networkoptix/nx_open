#pragma once

#include <QtCore/QElapsedTimer>

#include "p2p_fwd.h"
#include <nx/network/websocket/websocket.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/common/from_this_to_shared.h>
#include <core/resource/shared_resource_pointer.h>
#include <core/resource_access/user_access_data.h>
#include <transaction/connection_guard.h>
#include <nx/vms/api/data/tran_state_data.h>
#include <nx/network/http/http_async_client.h>
#include <transaction/abstract_transaction_transport.h>

namespace ec2 {
class QnAbstractTransaction;
}

namespace nx {
namespace p2p {

using namespace ec2;
class ConnectionBase;
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
        const vms::api::PeerDataEx& localPeer,
        const nx::utils::Url& remotePeerUrl,
        const std::chrono::seconds& keepAliveTimeout,
        std::unique_ptr<QObject> opaqueObject,
        std::unique_ptr<ConnectionLockGuard> connectionLockGuard = nullptr);

    ConnectionBase(
        const vms::api::PeerDataEx& remotePeer,
        const vms::api::PeerDataEx& localPeer,
        nx::network::WebSocketPtr webSocket,
        const QUrlQuery& requestUrlQuery,
        std::unique_ptr<QObject> opaqueObject,
        std::unique_ptr<ConnectionLockGuard> connectionLockGuard = nullptr);

    virtual ~ConnectionBase();

    static const SendCounters& sendCounters() { return m_sendCounters;  }

    /** Peer that opens this connection */
    Direction direction() const { return m_direction; }

    virtual const vms::api::PeerDataEx& localPeer() const override { return m_localPeer; }
    virtual const vms::api::PeerDataEx& remotePeer() const override { return m_remotePeer; }
    virtual bool isIncoming() const override { return m_direction == Direction::incoming;  }
    virtual nx::network::http::AuthInfoCache::AuthorizationCacheItem authData() const override;
    virtual std::multimap<QString, QString> httpQueryParams() const override;

    State state() const;
    virtual void setState(State state);

    void sendMessage(MessageType messageType, const nx::Buffer& data);
    void sendMessage(const nx::Buffer& data);

    void startConnection();
    void startReading();

    QObject* opaqueObject();

    virtual utils::Url remoteAddr() const override;
    const nx::network::WebSocket* webSocket() const;
    void stopWhileInAioThread();

    virtual bool validateRemotePeerData(const vms::api::PeerDataEx& /*peer*/) const { return true; }

    void addAdditionalRequestHeaders(nx_http::HttpHeaders headers);
    void addRequestQueryParams(std::vector<std::pair<QString, QString>> queryParams);

signals:
    void gotMessage(QWeakPointer<ConnectionBase> connection, nx::p2p::MessageType messageType, const QByteArray& payload);
    void stateChanged(QWeakPointer<ConnectionBase> connection, ConnectionBase::State state);
    void allDataSent(QWeakPointer<ConnectionBase> connection);

protected:
    virtual void fillAuthInfo(nx::network::http::AsyncClient* httpClient, bool authByKey) = 0;
    void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread);
    nx::network::WebSocket* webSocket();

private:
    void cancelConnecting(State state, const QString& reason);

    void onHttpClientDone();

    void onMessageSent(SystemError::ErrorCode errorCode, size_t bytesSent);
    void onNewMessageRead(SystemError::ErrorCode errorCode, size_t bytesRead);

    bool handleMessage(const nx::Buffer& message);
    int messageHeaderSize(bool isClient) const;
    MessageType getMessageType(const nx::Buffer& buffer, bool isClient) const;
private:
    enum class CredentialsSource
    {
        remoteUrl,
        serverKey,
        appserverConnectionFactory,
        none,
    };
protected:
    const Direction m_direction;
private:
    std::deque<nx::Buffer> m_dataToSend;
    QByteArray m_readBuffer;

    std::unique_ptr<nx::network::http::AsyncClient> m_httpClient;

    CredentialsSource m_credentialsSource = CredentialsSource::serverKey;

    vms::api::PeerDataEx m_remotePeer;
    vms::api::PeerDataEx m_localPeer;

    nx::network::WebSocketPtr m_webSocket;
    std::atomic<State> m_state{State::Connecting};

    nx::utils::Url m_remotePeerUrl;

    static SendCounters m_sendCounters;

    network::aio::Timer m_timer;
    std::chrono::seconds m_keepAliveTimeout;
    std::unique_ptr<QObject> m_opaqueObject;
    std::unique_ptr<ConnectionLockGuard> m_connectionLockGuard;

    int m_sendSequence = 0;
    int m_lastReceivedSequence = 0;

    nx::network::http::AuthInfoCache::AuthorizationCacheItem m_httpAuthCacheItem;
    mutable QnMutex m_mutex;

    nx_http::HttpHeaders m_additionalRequestHeaders;
    std::vector<std::pair<QString, QString>> m_requestQueryParams;
    std::multimap<QString, QString> m_remoteQueryParams;
};

QString toString(ConnectionBase::State value);

} // namespace p2p
} // namespace nx

Q_DECLARE_METATYPE(nx::p2p::P2pConnectionPtr)
Q_DECLARE_METATYPE(nx::p2p::ConnectionBase::State)
