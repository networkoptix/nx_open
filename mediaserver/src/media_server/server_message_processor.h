#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/resource_fwd.h>
#include "nx_ec/impl/ec_api_impl.h"

class QnServerMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnServerMessageProcessor();

#ifdef PROXY_STRICT_IP
    bool isKnownAddr(const QString& addr) const;
#endif
    virtual void updateResource(const QnResourcePtr &resource) override;

protected:
    virtual void onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status ) override;
    virtual void init(ec2::AbstractECConnectionPtr connection);
    virtual void afterRemovingResource(const QnId& id) override;
private slots:
    void at_remotePeerFound(QnId id, bool isClient, bool isProxy);
    void at_remotePeerLost(QnId id, bool isClient, bool isProxy);
private:
#ifdef PROXY_STRICT_IP
    void updateAllIPList(const QnId& id, const QList<QHostAddress>& addrList);
    void updateAllIPList(const QnId& id, const QList<QString>& addr);
    void updateAllIPList(const QnId& id, const QString& addr);
#endif
private:
#ifdef PROXY_STRICT_IP
    mutable QMutex m_mutexAddrList;
    QHash<QString, int> m_allIPAddress;
    QHash<QnId, QList<QString> > m_addrById;
#endif
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
