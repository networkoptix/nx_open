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
#include "connection_guard.h"
#include <nx_ec/data/api_tran_state_data.h>

namespace ec2 {

class P2pConnection;
class QnAbstractTransaction;
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
        const ApiPeerData& localPeer,
        ConnectionLockGuard connectionLockGuard,
        const QUrl& remotePeerUrl);
    P2pConnection(
        QnCommonModule* commonModule,
        const ApiPeerData& remotePeer,
        const ApiPeerData& localPeer,
        ConnectionLockGuard connectionLockGuard,
        const WebSocketPtr& webSocket);
    virtual ~P2pConnection();

    static const SendCounters& sendCounters() { return m_sendCounters;  }

    /** Peer that opens this connection */
    Direction direction() const;

    ApiPeerData localPeer() const;
    ApiPeerData remotePeer() const;

    State state() const;
    void setState(State state);

    void sendMessage(MessageType messageType, const nx::Buffer& data);
    void sendMessage(const nx::Buffer& data);

    qint64 remoteIdentityTime() const;

    ApiPersistentIdData decode(PeerNumberType shortPeerNumber) const;
    PeerNumberType encode(const ApiPersistentIdData& fullId, PeerNumberType shortPeerNumber = kUnknownPeerNumnber);

    void startConnection();
    void startReading();

    /** MiscData contains members that managed by P2pMessageBus. P2pConnection doesn't touch it */
    struct MiscData
    {
        MiscData()
        {
            localPeersTimer.invalidate();
        }

        // to local part
        QByteArray localPeersMessage; //< last sent peers message
        QElapsedTimer localPeersTimer; //< last sent peers time
        QVector<ApiPersistentIdData> localSubscription; //< local -> remote subscription
        bool isLocalStarted = false; //< we opened connection to remote peer

        // to remote part
        QByteArray remotePeersMessage; //< last received peers message
        QnTranState remoteSubscription; //< remote -> local subscription
        bool selectingDataInProgress = false;
        bool isRemoteStarted = false; //< remote peer has open logical connection to us

        QElapsedTimer sendStartTimer;
    };

    MiscData& miscData();

    const Qn::UserAccessData& getUserAccessData() const { return m_userAccessData; }
    bool remotePeerSubscribedTo(const ApiPersistentIdData& peer) const;
    bool updateSequence(const QnAbstractTransaction& tran);
    bool localPeerSubscribedTo(const ApiPersistentIdData& peer) const;
signals:
    void gotMessage(P2pConnectionPtr connection, MessageType messageType, const QByteArray& payload);
    void stateChanged(P2pConnectionPtr connection, P2pConnection::State state);
    void allDataSent(P2pConnectionPtr connection);
private:
    void cancelConnecting();

    void onResponseReceived(const nx_http::AsyncHttpClientPtr& client);
    void onHttpClientDone(const nx_http::AsyncHttpClientPtr& httpClient);

    void fillAuthInfo(const nx_http::AsyncHttpClientPtr& httpClient, bool authByKey);
    void setRemoteIdentityTime(qint64 time);

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
    MiscData m_miscData;
    qint64 m_remoteIdentityTime = 0;
    QUrl m_remotePeerUrl;
    const Qn::UserAccessData m_userAccessData = Qn::kSystemAccess;

    PeerNumberInfo m_shortPeerInfo;
    std::unique_ptr<ConnectionLockGuard> m_connectionLockGuard;

    int m_remotePeerEcProtoVersion = nx_ec::INITIAL_EC2_PROTO_VERSION;
    int m_localPeerProtocolVersion = nx_ec::EC2_PROTO_VERSION;

    static SendCounters m_sendCounters;
};

Q_DECLARE_METATYPE(P2pConnectionPtr)
Q_DECLARE_METATYPE(P2pConnection::State)

const char* toString(MessageType value);
const char* toString(P2pConnection::State value);

} // namespace ec2
