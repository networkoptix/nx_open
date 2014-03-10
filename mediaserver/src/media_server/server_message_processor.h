#ifndef QN_SERVER_MESSAGE_PROCESSOR_H
#define QN_SERVER_MESSAGE_PROCESSOR_H

#include <api/common_message_processor.h>

#include <core/resource/resource.h>
#include "nx_ec/impl/ec_api_impl.h"

class QnServerMessageProcessor : public QnCommonMessageProcessor
{
    Q_OBJECT

    typedef QnCommonMessageProcessor base_type;
public:
    QnServerMessageProcessor();

    bool isKnownAddr(const QString& addr) const;
    virtual void updateResource(QnResourcePtr resource) override;

protected:
    virtual void onResourceStatusChanged(QnResourcePtr , QnResource::Status ) override;
    virtual void onGotInitialNotification(const ec2::QnFullResourceData& fullData) override;
    virtual void init(ec2::AbstractECConnectionPtr connection);
    virtual void afterRemovingResource(const QnId& id) override;
private slots:
    void at_remotePeerFound(QnId id, bool isClient, bool isProxy);
    void at_remotePeerLost(QnId id, bool isClient, bool isProxy);
private:
    void updateAllIPList(const QnId& id, const QList<QHostAddress>& addrList);
    void updateAllIPList(const QnId& id, const QList<QString>& addr);
    void updateAllIPList(const QnId& id, const QString& addr);
private:
    mutable QMutex m_mutexAddrList;
    QHash<QString, int> m_allIPAddress;
    QHash<QnId, QList<QString> > m_addrById;
};

#endif // QN_SERVER_MESSAGE_PROCESSOR_H
