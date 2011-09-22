#ifndef _SESSION_MANAGER_H
#define _SESSION_MANAGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>

#include "Objects.h"

class SessionManager : public QObject
{
    Q_OBJECT

public:
    SessionManager(const QString& host, const QString& login, const QString& password);

    void openEventChannel();
    void closeEventChannel();

    int getResourceTypes();
    int getCameras();

    int addServer(const Server&);
    int addCamera(const Camera&, const QString& serverId);

Q_SIGNALS:
    void eventsReceived(QList<Event*>* events);
    void camerasReceived(int requestId, QList<Camera*>* cameras);
    void serversReceived(int requestId, QList<Server*>* servers);
    void resourceTypesReceived(int requestId, QList<ResourceType*>* resourceTypes);

    void eventOccured(QString data);

    void error(int requestId, QString message);

private:
    int addObject(const Object& object, const QString& additionalArgs = "");
    int getObjectList(QString objectName);

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
