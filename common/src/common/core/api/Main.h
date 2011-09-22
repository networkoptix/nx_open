#ifndef _MAIN_H
#define _MAIN_H

#include <QString>
#include <QList>

#include "SessionManager.h"
#include "Objects.h"

enum State
{
    INITIAL,
    GOT_RESOURCE_TYPES,
    GOT_SERVER_ID,
    GOT_CAMERAS,
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
    void eventsReceived(QList<Event*>* events);
    void camerasReceived(int requestId, QList<Camera*>* cameras);
    void serversReceived(int requestId, QList<Server*>* servers);
    void resourceTypesReceived(int requestId, QList<ResourceType*>* resourceTypes);
    void error(int requestId, QString message);

private:
    void nextStep();

private:
    QString m_serverId;

    SessionManager sm;
    int resourceTypesRequestId;
    int camerasRequestId;
    int addCameraRequestId;
    int addServerRequestId;

    State m_state;

    QList<ResourceType*>* m_resourceTypes;
};

#endif // _MAIN_H
