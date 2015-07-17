#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/resource_fwd.h>
#include "nx_ec/impl/ec_api_impl.h"
#include "nx_ec/data/api_server_alive_data.h"
#include <utils/network/http/httptypes.h>

class QHostAddress;

class QnServerMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnServerMessageProcessor();

    virtual void updateResource(const QnResourcePtr &resource) override;
    bool isProxy(const nx_http::Request& request) const;
    void registerProxySender(QnUniversalTcpListener* tcpListener);

protected:
    virtual void connectToConnection(const ec2::AbstractECConnectionPtr &connection) override;
    virtual void disconnectFromConnection(const ec2::AbstractECConnectionPtr &connection) override;

    virtual void handleRemotePeerFound(const ec2::ApiPeerAliveData &data) override;
    virtual void handleRemotePeerLost(const ec2::ApiPeerAliveData &data) override;

    virtual void onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus ) override;
    virtual void init(const ec2::AbstractECConnectionPtr& connection) override;
    virtual void afterRemovingResource(const QnUuid& id) override;
    void execBusinessActionInternal(const QnAbstractBusinessActionPtr& action) override;
    bool isLocalAddress(const QString& addr) const;
    virtual bool canRemoveResource(const QnUuid& resourceId) override;
    virtual void removeResourceIgnored(const QnUuid& resourceId) override;

private slots:
    void at_updateChunkReceived(const QString &updateId, const QByteArray &data, qint64 offset);
    void at_updateInstallationRequested(const QString &updateId);

    void at_reverseConnectionRequested(const ec2::ApiReverseConnectionData &data);

    void at_remotePeerUnauthorized(const QnUuid& id);

private:
    mutable QMutex m_mutexAddrList;
    const int m_serverPort;
    QnUniversalTcpListener* m_universalTcpListener;
    mutable QnMediaServerResourcePtr m_mServer;
    QSet<QnUuid> m_delayedOnlineStatus;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
