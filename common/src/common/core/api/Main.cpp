#include <QtCore/QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>

#include "Main.h"
#include "SessionManager.h"

namespace
{
    QString readServerId()
    {
        QFile file("serverid");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();

        QTextStream in(&file);
        return in.readLine();
    }

    void writeServerId(const QString& serverId)
    {
        QFile file("serverid");
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qDebug("Error opening file for writing");
            return;
        }

        QTextStream out(&file);
        out << serverId;
    }
}

Client::Client(const QString& host, const QString& login, const QString& password)
    : sm(host, login, password)
{
    m_serverId = readServerId();

    connect(&sm, SIGNAL(resourceTypesReceived(int, QList<ResourceType*>*)), this, SLOT(resourceTypesReceived(int, QList<ResourceType*>*)));
    connect(&sm, SIGNAL(camerasReceived(int, QList<Camera*>*)), this, SLOT(camerasReceived(int, QList<Camera*>*)));
    connect(&sm, SIGNAL(serversReceived(int, QList<Server*>*)), this, SLOT(serversReceived(int, QList<Server*>*)));
    connect(&sm, SIGNAL(eventsReceived(QList<Event*>*)), this, SLOT(eventsReceived(QList<Event*>*)));
}

Client::~Client()
{
    if (m_resourceTypes)
    {
        foreach(ResourceType* resourceType, *m_resourceTypes)
            delete resourceType;

        delete m_resourceTypes;
    }
}

void Client::run()
{
    // sm.openEventChannel();

    m_state = INITIAL;
    nextStep();
}

void Client::eventsReceived(QList<Event*>* events)
{
    int n = 0;
    foreach(Event* event, *events)
    {
        qDebug() << "Event " << n++ << ": " << event->toString();
        delete event;
    }

    delete events;
}

void Client::camerasReceived(int requestId, QList<Camera*>* cameras)
{
    if (requestId == camerasRequestId)
    {
        qDebug("cameras received");

        int n = 0;
        foreach(Camera* camera, *cameras)
        {
            qDebug() << "Camera " << n++ << ": " << camera->toString();
            delete camera;
        }

        delete cameras;

        m_state = GOT_CAMERAS;
        nextStep();
    } else if (requestId == addCameraRequestId)
    {
        qDebug("camera received");

        int n = 0;
        foreach(Camera* camera, *cameras)
        {
            qDebug() << "Camera " << n++ << ": " << camera->toString();
            delete camera;
        }

        delete cameras;

        m_state = ADDED_CAMERA;
        nextStep();
    }
}

void Client::serversReceived(int requestId, QList<Server *> *servers)
{
    if (requestId != addServerRequestId)
        return;

    if (servers->size() != 1)
    {
        qDebug("Error. Returned > 1 servers.");
        return;
    }

    Server* server = servers->at(0);
    m_serverId = server->id();
    writeServerId(m_serverId);
    delete server;
    delete servers;

    m_state = GOT_SERVER_ID;
    nextStep();
}

void Client::resourceTypesReceived(int requestId, QList<ResourceType*>* resourceTypes)
{
    if (requestId != resourceTypesRequestId)
        return;

    qDebug("resource types received");

    m_resourceTypes = resourceTypes;

    int n = 0;
    foreach(ResourceType* resourceType, *resourceTypes)
    {
        qDebug() << "ResourceType " << n++ << ": " << resourceType->toString();
    }

    m_state = GOT_RESOURCE_TYPES;
    nextStep();
}

void Client::error(int requestId, QString message)
{
    qDebug("error");
}

void Client::nextStep()
{
    switch (m_state)
    {
    case INITIAL:
        resourceTypesRequestId = sm.getResourceTypes();
        break;

    case GOT_RESOURCE_TYPES:
        if (!m_serverId.isEmpty())
        {
            m_state = GOT_SERVER_ID;
            nextStep();
        } else
        {
            Server server;
            server.setType(m_resourceTypes->at(0)->id());
            server.setName("TestServer");
            server.setUrl("rtsp://someserver");
            server.setApiUrl("http://someserver");

            addServerRequestId = sm.addServer(server);
            break;
        }
        break;

    case GOT_SERVER_ID:
        camerasRequestId = sm.getCameras();
        break;

    case GOT_CAMERAS:
        Camera camera;
        camera.setName("Qt Camera");
        camera.setType(m_resourceTypes->at(0)->id());

        addCameraRequestId = sm.addCamera(camera, m_serverId);
        break;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Client client(QString("localhost"), QString("ivan"), QString("225653"));
    client.run();

    return a.exec();
}
