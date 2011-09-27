#ifndef _SESSION_MANAGER_H
#define _SESSION_MANAGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>

#include "Objects.h"

typedef long RequestId;

class SessionManager : public QObject
{
    Q_OBJECT

public:
    SessionManager(const QString& host, const QString& login, const QString& password);

    void openEventChannel();
    void closeEventChannel();

    RequestId getResourceTypes();
    RequestId getCameras();
    RequestId getLayouts();
    RequestId getServers();
    RequestId getResources(bool tree);

    RequestId addServer(const Server&);
    RequestId addLayout(const Layout&);
    RequestId addCamera(const Camera&, const QString& serverId);

Q_SIGNALS:
    void eventsReceived(QList<Event*>* events);
    void camerasReceived(RequestId requestId, QList<Camera*>* cameras);
    void serversReceived(RequestId requestId, QList<Server*>* servers);
    void layoutsReceived(RequestId requestId, QList<Layout*>* servers);
    void resourceTypesReceived(RequestId requestId, QList<ResourceType*>* resourceTypes);
    void resourcesReceived(RequestId requestId, QList<Resource*>* resources);

    void eventOccured(QString data);

    void error(int requestId, QString message);

private:
    RequestId addObject(const Object& object, const QString& additionalArgs = "");
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
