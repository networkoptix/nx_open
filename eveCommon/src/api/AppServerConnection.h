#ifndef APPSERVERCONNECTION_H
#define APPSERVERCONNECTION_H

#include "device/resource_type.h"
#include "device/qnresource.h"

#include "SessionManager.h"

class QnAppServerAdapter : public QObject
{
    Q_OBJECT
public:
    QnAppServerAdapter(const QString& host, const QString& login, const QString& password);

    RequestId getResourceTypes();

    RequestId getServers();
    RequestId getCameras();
    RequestId getLayouts();

    RequestId getResources();

    RequestId addServer(const QnResource&);
    RequestId addCamera(const QnResource&, const QnId& serverId);
    RequestId addLayout(const QnResource&);

Q_SIGNALS:
    void resourceTypesReceived(RequestId requestId, QList<QnResourceTypePtr> resourceTypes);
    void resourcesReceived(RequestId requestId, QList<QnResourcePtr> resources);
    void serversReceived(RequestId requestId, QList<QnResourcePtr> servers);
    void layoutsReceived(RequestId requestId, QList<QnResourcePtr> layouts);
    void camerasReceived(RequestId requestId, QList<QnResourcePtr> cameras);

    void error(RequestId requestId, QString message);

private slots:
    void xsdResourceTypesReceived(RequestId requestId, QnApiResourceTypeResponsePtr xsdResourceTypes);
    void xsdResourcesReceived(RequestId requestId, QnApiResourceResponsePtr xsdResources);

    void xsdServersReceived(RequestId requestId, QnApiServerResponsePtr servers);
    void xsdLayoutsReceived(RequestId requestId, QnApiLayoutResponsePtr layouts);
    void xsdCamerasReceived(RequestId requestId, QnApiCameraResponsePtr xsdCameras);

    void xsdError(RequestId requestId, QString message);

private:
    SessionManager m_sessionManager;
};

#endif // APPSERVERCONNECTION_H
