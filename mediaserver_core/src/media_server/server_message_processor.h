#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/resource_fwd.h>
#include "nx_ec/impl/ec_api_impl.h"
#include "nx_ec/data/api_peer_alive_data.h"
#include <nx/network/http/http_types.h>
#include "network/universal_tcp_listener.h"


class QHostAddress;

class QnServerMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnServerMessageProcessor(QnCommonModule* commonModule);

    virtual void updateResource(const QnResourcePtr &resource, ec2::NotificationSource source) override;
    void registerProxySender(QnUniversalTcpListener* tcpListener);

    void startReceivingLocalNotifications(const ec2::AbstractECConnectionPtr &connection);
protected:
    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection) override;
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) override;

    virtual void handleRemotePeerFound(QnUuid peer, Qn::PeerType peerType) override;
    virtual void handleRemotePeerLost(QnUuid peer, Qn::PeerType peerType) override;

    virtual void onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus, ec2::NotificationSource source) override;
    virtual void init(const ec2::AbstractECConnectionPtr& connection) override;
    void execBusinessActionInternal(const QnAbstractBusinessActionPtr& action) override;
    bool isLocalAddress(const QString& addr) const;

    /**
     * Check if the resource can be safely removed by transaction from other server.
     * Common scenario is to allow to remove all resources.
     * Known exceptions are:
     * * Forbid to remove the current server. Really, if it is running and located in the system
     *   it should be found again instantly.
     * * Forbid to remove local server storages. Our server should make this decision himself.
     */
    virtual bool canRemoveResource(const QnUuid& resourceId) override;

    /**
     *  If by any way our resource was removed (e.g. somebody removed our server while it was offline)
     *  we need to restore status-quo. This method sends corresponding transactions.
     */
    virtual void removeResourceIgnored(const QnUuid& resourceId) override;

    virtual QnResourceFactory* getResourceFactory() const override;
private slots:
    void at_updateChunkReceived(const QString &updateId, const QByteArray &data, qint64 offset);
    void at_updateInstallationRequested(const QString &updateId);

    void at_reverseConnectionRequested(const ec2::ApiReverseConnectionData &data);

    void at_remotePeerUnauthorized(const QnUuid& id);

private:
    mutable QnMutex m_mutexAddrList;
    const int m_serverPort;
    QnUniversalTcpListener* m_universalTcpListener;
    mutable QnMediaServerResourcePtr m_mServer;
    QSet<QnUuid> m_delayedOnlineStatus;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
