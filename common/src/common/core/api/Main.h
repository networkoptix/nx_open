#ifndef _MAIN_H
#define _MAIN_H

#include <QString>
#include <QList>

#include "SessionManager.h"
// #include "Objects.h"

enum State
{
    INITIAL,
    GOT_RESOURCE_TYPES,
    GOT_SERVER_ID,
    GOT_CAMERAS,
    GOT_LAYOUTS,
    GOT_SERVERS,
    GOT_RESOURCES,
    ADDED_CAMERA
};

class Client : public QObject
{
    Q_OBJECT

public:
    Client(const QString& host, const QString& login, const QString& password);
    ~Client();
    
    void run();

private slots:
    void eventsReceived(QnApiEventResponsePtr events);
    void camerasReceived(RequestId requestId, QnApiCameraResponsePtr cameras);
    void serverAdded(RequestId requestId, QnApiServerResponsePtr servers);
    void serversReceived(RequestId requestId, QnApiServerResponsePtr servers);
    void layoutsReceived(RequestId requestId, QnApiLayoutResponsePtr layouts);
    void resourceTypesReceived(RequestId requestId, QnApiResourceTypeResponsePtr resourceTypes);
    void resourcesReceived(RequestId requestId, QnApiResourceResponsePtr resources);

    void error(RequestId requestId, QString message);

private:
    void nextStep();

private:
    QString m_serverId;

    SessionManager sm;
    RequestId resourceTypesRequestId;
    RequestId camerasRequestId;
    RequestId addCameraRequestId;
    RequestId addServerRequestId;
    RequestId serversRequestId;
    RequestId layoutsRequestId;
    RequestId resourcesRequestId;

    State m_state;

    QnApiResourceTypeResponsePtr m_resourceTypes;
};

#endif // _MAIN_H
