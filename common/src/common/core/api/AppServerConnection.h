#ifndef APPSERVERCONNECTION_H
#define APPSERVERCONNECTION_H

#include "SessionManager.h"

#if 0
class QnAppServerAdapter : public QObject
{
    Q_OBJECT
public:
    QnAppServerAdapter(const QString& host, const QString& login, const QString& password);

    void openEventChannel();
    void closeEventChannel();

    RequestId getResourceTypes();
    RequestId getResources(bool tree);

    RequestId getServers();
    RequestId getLayouts();
    RequestId getCameras();

    RequestId addServer(const QnServer&);
    RequestId addLayout(const QnLayout&);
    RequestId addCamera(const QnCamera&, const QString& serverId);

Q_SIGNALS:
    void eventsReceived(QnApiEventResponsePtr events);

    void resourceTypesReceived(RequestId requestId, QList<QnResourceTypePtr> resourceTypes);
    void resourcesReceived(RequestId requestId, QList<QnResourcePtr> resources);

    void serversReceived(RequestId requestId, QList<QnResourcePtr> servers);
    void layoutsReceived(RequestId requestId, QList<QnResourcePtr> layouts);
    void camerasReceived(RequestId requestId, QList<QnResourcePtr> cameras);

    void eventOccured(QString data);

    void error(int requestId, QString message);

private slots:
    void resourceTypesReceived(RequestId requestId, QnApiResourceTypeResponsePtr resourceTypes);
    void resourcesReceived(RequestId requestId, QnApiResourceResponsePtr resources);

    void serversReceived(RequestId requestId, QnApiServerResponsePtr servers);
    void layoutsReceived(RequestId requestId, QnApiLayoutResponsePtr layouts);
    void camerasReceived(RequestId requestId, QnApiCameraResponsePtr cameras);

    void serverAdded(RequestId requestId, QnApiServerResponsePtr servers);

    void eventsReceived(QnApiEventResponsePtr events);

private:
    SessionManager m_sessionManager;
};
#endif

#endif // APPSERVERCONNECTION_H
