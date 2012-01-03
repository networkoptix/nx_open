#ifndef _APP_SESSION_MANAGER_H
#define _APP_SESSION_MANAGER_H

#include <QtCore/QSharedPointer>

#include "api/Types.h"
#include "utils/network/simple_http_client.h"
#include "utils/common/qnid.h"

#include "SessionManager.h"

class AppSessionManager : public SessionManager
{
    Q_OBJECT

public:
    AppSessionManager(const QUrl &url);

    int getResourceTypes(QnApiResourceTypeResponsePtr& resourceTypes);
    int getResources(QnApiResourceResponsePtr& resources);

    int getCameras(QnApiCameraResponsePtr& scheduleTasks, const QnId& mediaServerId);
    int getStorages(QnApiStorageResponsePtr& resources);

    int addServer(const ::xsd::api::servers::Server&, QnApiServerResponsePtr& servers);
    int addCamera(const ::xsd::api::cameras::Camera&, QnApiCameraResponsePtr& cameras);
    int addStorage(const ::xsd::api::storages::Storage&);

private:
    int addObject(const QString& objectName, const QByteArray& body, QByteArray& response);

private:
    static const unsigned long XSD_FLAGS = xml_schema::flags::dont_initialize | xml_schema::flags::dont_validate;
};

#endif // _APP_SESSION_MANAGER_H
