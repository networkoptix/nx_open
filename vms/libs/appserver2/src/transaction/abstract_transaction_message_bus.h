// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QThread>
#include <QtNetwork/QHostAddress>

#include <nx/network/http/auth_tools.h>
#include <nx/network/ssl/helpers.h>
#include <nx/vms/common/system_context_aware.h>
#include <utils/common/enable_multi_thread_direct_connection.h>

#include "connection_guard_shared_state.h"
#include "transport_connection_info.h"

namespace nx { namespace network { class SocketAddress; }}

namespace ec2 {

class QnJsonTransactionSerializer;
class QnUbjsonTransactionSerializer;
class QnAbstractTransactionTransport;
class TimeSynchronizationManager;

class ECConnectionNotificationManager;
namespace detail { class QnDbManager; }

class AbstractTransactionMessageBus:
    public QObject,
    public nx::vms::common::SystemContextAware,
    public EnableMultiThreadDirectConnection<AbstractTransactionMessageBus>
{
    Q_OBJECT;
public:
    AbstractTransactionMessageBus(nx::vms::common::SystemContext* value):
        nx::vms::common::SystemContextAware(value)
    {
    }

    virtual ~AbstractTransactionMessageBus() {}

    virtual void start() = 0;
    virtual void stop() = 0;

    virtual QSet<QnUuid> directlyConnectedClientPeers() const = 0;
    virtual QSet<QnUuid> directlyConnectedServerPeers() const = 0;

    virtual QnUuid routeToPeerVia(
        const QnUuid& dstPeer, int* distance, nx::network::SocketAddress* knownPeerAddress) const = 0;
    virtual int distanceToPeer(const QnUuid& dstPeer) const = 0;

    virtual void addOutgoingConnectionToPeer(
        const QnUuid& id,
        nx::vms::api::PeerType peerType,
        const nx::utils::Url& url,
        std::optional<nx::network::http::Credentials> credentials = std::nullopt,
        nx::network::ssl::AdapterFunc adapterFunc = nx::network::ssl::kDefaultCertificateCheck) = 0;
    virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) = 0;
    virtual void updateOutgoingConnection(
        const QnUuid& id, nx::network::http::Credentials credentials) = 0;

    virtual void dropConnections() = 0;

    virtual ConnectionInfos connectionInfos() const = 0;

    virtual void setHandler(ECConnectionNotificationManager* handler) = 0;
    virtual void removeHandler(ECConnectionNotificationManager* handler) = 0;

    virtual QnJsonTransactionSerializer* jsonTranSerializer() const = 0;
    virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const = 0;

    virtual ConnectionGuardSharedState* connectionGuardSharedState() = 0;

signals:
    void peerFound(QnUuid data, nx::vms::api::PeerType peerType);
    void peerLost(QnUuid data, nx::vms::api::PeerType peerType);
    void remotePeerUnauthorized(QnUuid id);
    void remotePeerForbidden(QnUuid id, const QString& message);
    void remotePeerHandshakeError(QnUuid id);
    void stateChanged(const QnUuid& id, const QString& state);

    /** Emitted on a new direct connection to a remote peer has been established */
    void newDirectConnectionEstablished(QnAbstractTransactionTransport* transport);
};

} // namespace ec2
