#ifndef _SESSION_MANAGER_H
#define _SESSION_MANAGER_H

#include <QString>
#include <QList>
#include <QAuthenticator>
#include <QSharedPointer>

#include "utils/network/simple_http_client.h"
#include "api/Types.h"

class SessionManager
{
public:
    SessionManager(const QHostAddress& host, const QAuthenticator& auth);

    int getResourceTypes(QnApiResourceTypeResponsePtr& resourceTypes);
    int getResources(QnApiResourceResponsePtr& resources);

    int addServer(const ::xsd::api::servers::Server&, QnApiServerResponsePtr& servers);
    int addCamera(const ::xsd::api::cameras::Camera&, QnApiCameraResponsePtr& cameras);

private:
    int addObject(const QString& objectName, const QByteArray& body, QByteArray& response);

private:
    int getObjectList(QString objectName, QByteArray& reply);

private:
    CLSimpleHTTPClient m_client;

    bool m_needEvents;
};

#endif // _SESSION_MANAGER_H
