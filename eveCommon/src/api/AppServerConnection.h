#ifndef APPSERVERCONNECTION_H
#define APPSERVERCONNECTION_H

#include "device/resource_type.h"
#include "device/qnresource.h"

typedef long RequestId;

class QN_EXPORT QnAppServerAdapter : public QObject
{
    Q_OBJECT
public:
    virtual RequestId getResourceTypes() = 0;

    virtual RequestId getResources() = 0;

    virtual RequestId addServer(const QnResource&) = 0;
    virtual RequestId addCamera(const QnResource&, const QnId& serverId) = 0;

Q_SIGNALS:
    void resourceTypesReceived(RequestId requestId, QList<QnResourceTypePtr> resourceTypes);
    void resourcesReceived(RequestId requestId, QList<QnResourcePtr> resources);
    void serversReceived(RequestId requestId, QList<QnResourcePtr> servers);
    void layoutsReceived(RequestId requestId, QList<QnResourcePtr> layouts);
    void camerasReceived(RequestId requestId, QList<QnResourcePtr> cameras);

    void error(RequestId requestId, QString message);

protected:
    QnAppServerAdapter() {}
    QnAppServerAdapter(const QnAppServerAdapter&) {}
};

QN_EXPORT QnAppServerAdapter* createServerAdapter(const QString& host, const QString& login, const QString& password);

#endif // APPSERVERCONNECTION_H
