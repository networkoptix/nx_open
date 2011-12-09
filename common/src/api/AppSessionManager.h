#ifndef _APP_SESSION_MANAGER_H
#define _APP_SESSION_MANAGER_H

#include <QString>
#include <QList>
#include <QAuthenticator>
#include <QSharedPointer>

#include "utils/network/simple_http_client.h"
#include "api/Types.h"
#include "SessionManager.h"

class AppSessionManager: public SessionManager
{
public:
    AppSessionManager(const QHostAddress& host, int port, const QAuthenticator& auth);

    int getResourceTypes(QnApiResourceTypeResponsePtr& resourceTypes);
    int getResources(QnApiResourceResponsePtr& resources);

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
