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

    connect(&sm, SIGNAL(resourceTypesReceived(RequestId, QList<ResourceType*>*)), this, SLOT(resourceTypesReceived(RequestId, QList<ResourceType*>*)));
    connect(&sm, SIGNAL(camerasReceived(RequestId, QList<Camera*>*)), this, SLOT(camerasReceived(RequestId, QList<Camera*>*)));
    connect(&sm, SIGNAL(serversReceived(RequestId, QList<Server*>*)), this, SLOT(serversReceived(RequestId, QList<Server*>*)));
    connect(&sm, SIGNAL(layoutsReceived(RequestId, QList<Layout*>*)), this, SLOT(layoutsReceived(RequestId, QList<Layout*>*)));
    connect(&sm, SIGNAL(resourcesReceived(RequestId, QList<Resource*>*)), this, SLOT(resourcesReceived(RequestId, QList<Resource*>*)));
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

void Client::camerasReceived(RequestId requestId, QList<Camera*>* cameras)
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

void Client::serverAdded(RequestId requestId, QList<Server *> *servers)
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

void Client::layoutsReceived(RequestId requestId, QList<Layout *> *layouts)
{
    if (requestId != layoutsRequestId)
        return;

    foreach (Layout* layout, *layouts)
    {
        qDebug() << "Layoout: " << layout->name();
        delete layout;
    }

    delete layouts;

    m_state = GOT_LAYOUTS;
    nextStep();
}

void Client::serversReceived(RequestId requestId, QList<Server *> *servers)
{
    if (requestId != serversRequestId)
        return;

    foreach (Server* server, *servers)
    {
        qDebug() << "Server: " << server->name();
        delete server;
    }

    delete servers;

    m_state = GOT_SERVERS;
    nextStep();
}

void Client::resourcesReceived(RequestId requestId, QList<Resource *> *resources)
{
    if (requestId != resourcesRequestId)
        return;

    foreach (Resource* resource, *resources)
    {
        qDebug() << "Resource: " << resource->name();
        delete resource;
    }

    delete resources;

    m_state = GOT_RESOURCES;
    nextStep();
}

void Client::resourceTypesReceived(RequestId requestId, QList<ResourceType*>* resourceTypes)
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

void Client::error(RequestId requestId, QString message)
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
        layoutsRequestId = sm.getLayouts();
        break;

    case GOT_LAYOUTS:
        serversRequestId = sm.getServers();
        break;

    case GOT_SERVERS:
        resourcesRequestId = sm.getResources(true);
        break;

    case GOT_RESOURCES:
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
