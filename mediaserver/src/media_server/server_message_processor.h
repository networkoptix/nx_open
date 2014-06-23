#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/resource_fwd.h>
#include "nx_ec/impl/ec_api_impl.h"
#include "nx_ec/data/api_server_alive_data.h"
#include <utils/network/http/httptypes.h>


class QnServerMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnServerMessageProcessor();

    bool isKnownAddr(const QString& addr) const;
    virtual void updateResource(const QnResourcePtr &resource) override;
    bool isProxy(const nx_http::Request& request) const;
protected:
    virtual void onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status ) override;
    virtual void init(ec2::AbstractECConnectionPtr connection);
    virtual void afterRemovingResource(const QnId& id) override;
    void execBusinessActionInternal(QnAbstractBusinessActionPtr action) override;
    bool isLocalAddress(const QString& addr) const;
private slots:
    void at_remotePeerFound(ec2::ApiPeerAliveData data, bool isProxy);
    void at_remotePeerLost(ec2::ApiPeerAliveData data, bool isProxy);

    void at_updateChunkReceived(const QString &updateId, const QByteArray &data, qint64 offset);
    void at_updateInstallationRequested(const QString &updateId);

    void at_systemNameChangeRequested(const QString &systemName);

private:
    void updateAllIPList(const QnId& id, const QList<QHostAddress>& addrList);
    void updateAllIPList(const QnId& id, const QList<QString>& addr);
    void updateAllIPList(const QnId& id, const QString& addr);
    void removeIPList(const QnId& id);
private:
    mutable QMutex m_mutexAddrList;
    QHash<QString, int> m_allIPAddress;
    QHash<QnId, QList<QString> > m_addrById;
    const int m_serverPort;
    mutable QnMediaServerResourcePtr m_mServer;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
