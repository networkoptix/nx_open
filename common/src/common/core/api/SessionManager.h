#ifndef _SESSION_MANAGER_H
#define _SESSION_MANAGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QSharedPointer>

#include "generated/resourceTypes.h"
#include "generated/resources.h"
#include "generated/layouts.h"
#include "generated/cameras.h"
#include "generated/servers.h"
#include "generated/events.h"

// #include "Objects.h"

typedef long RequestId;

typedef QSharedPointer<xsd::api::resourceTypes::resourceTypes_t> QnApiResourceTypeResponsePtr;
typedef QSharedPointer<xsd::api::resources::resources_t>         QnApiResourceResponsePtr;
typedef QSharedPointer<xsd::api::layouts::layouts_t>             QnApiLayoutResponsePtr;
typedef QSharedPointer<xsd::api::cameras::cameras_t>             QnApiCameraResponsePtr;
typedef QSharedPointer<xsd::api::servers::servers_t>             QnApiServerResponsePtr;

typedef QSharedPointer<xsd::api::events::events_t>               QnApiEventResponsePtr;

class SessionManager : public QObject
{
    Q_OBJECT
public:
    SessionManager(const QString& host, const QString& login, const QString& password);

#if 0
    void openEventChannel();
    void closeEventChannel();
#endif

    RequestId getResourceTypes();

#if 0
    RequestId getResources(bool tree);

    RequestId getServers();
    RequestId getLayouts();
    RequestId getCameras();

    RequestId addServer(const ::xsd::api::servers::server&);
    RequestId addLayout(const ::xsd::api::layouts::layout&);
    RequestId addCamera(const ::xsd::api::cameras::camera&, const QString& serverId);
#endif

Q_SIGNALS:
    void resourceTypesReceived(RequestId requestId, QnApiResourceTypeResponsePtr resourceTypes);

#if 0
    void resourcesReceived(RequestId requestId, QnApiResourceResponsePtr resources);

    void serversReceived(RequestId requestId, QnApiServerResponsePtr servers);
    void layoutsReceived(RequestId requestId, QnApiLayoutResponsePtr servers);
    void camerasReceived(RequestId requestId, QnApiCameraResponsePtr cameras);

    void eventsReceived(QnApiEventResponsePtr events);

    void eventOccured(QString data);

    void error(int requestId, QString message);

private:
    RequestId addObject(const Object& object, const QString& additionalArgs = "");
#endif
private:
    RequestId getObjectList(QString objectName, bool tree);

private slots:
    void slotRequestFinished(QNetworkReply* reply);

private:
    QMap<QNetworkReply*, QString> m_requestObjectMap;

    QNetworkAccessManager m_manager;

    QString m_host;
    int m_port;
    QString m_login;
    QString m_password;

    bool m_needEvents;
};

#endif // _SESSION_MANAGER_H
