#ifndef _SESSION_MANAGER_H
#define _SESSION_MANAGER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QSharedPointer>

#include "api/Types.h"

typedef long RequestId;

class SessionManager : public QObject
{
    Q_OBJECT
public:
    SessionManager(const QString& host, const QString& login, const QString& password);

    RequestId getResourceTypes();
    RequestId getResources();

    RequestId addServer(const ::xsd::api::servers::Server&);
    RequestId addCamera(const ::xsd::api::cameras::Camera&);

private:
    RequestId addObject(const QString& objectName, const QByteArray& body);

Q_SIGNALS:
    void resourceTypesReceived(RequestId requestId, QnApiResourceTypeResponsePtr resourceTypes);
    void resourcesReceived(RequestId requestId, QnApiResourceResponsePtr resources);
    void camerasReceived(RequestId requestId, QnApiCameraResponsePtr cameras);
    void serversReceived(RequestId requestId, QnApiServerResponsePtr cameras);
    void layoutsReceived(RequestId requestId, QnApiLayoutResponsePtr cameras);

    void error(RequestId requestId, QString message);

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
