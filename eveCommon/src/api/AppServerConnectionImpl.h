#ifndef APPSERVERCONNECTIONIMPL_H
#define APPSERVERCONNECTIONIMPL_H

#include "SessionManager.h"
#include "Types.h"
#include "AppServerConnection.h"

class QnAppServerAdapterImpl : public QnAppServerAdapter
{
    Q_OBJECT
public:
    QnAppServerAdapterImpl(const QString& host, const QString& login, const QString& password);

    RequestId getResourceTypes();

    RequestId getResources();

    RequestId addServer(const QnResource&);
    RequestId addCamera(const QnResource&, const QnId& serverId);
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

#endif // APPSERVERCONNECTIONIMPL_H
