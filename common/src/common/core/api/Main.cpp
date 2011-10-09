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

    connect(&sm, SIGNAL(resourceTypesReceived(RequestId, QnApiResourceTypeResponsePtr)), this, SLOT(resourceTypesReceived(RequestId, QnApiResourceTypeResponsePtr)));
    connect(&sm, SIGNAL(camerasReceived(RequestId, QnApiCameraResponsePtr)), this, SLOT(camerasReceived(RequestId, QnApiCameraResponsePtr)));
    connect(&sm, SIGNAL(serversReceived(RequestId, QnApiServerResponsePtr)), this, SLOT(serversReceived(RequestId, QnApiServerResponsePtr)));
    connect(&sm, SIGNAL(layoutsReceived(RequestId, QnApiLayoutResponsePtr)), this, SLOT(layoutsReceived(RequestId, QnApiLayoutResponsePtr)));
    connect(&sm, SIGNAL(resourcesReceived(RequestId, QnApiResourceResponsePtr)), this, SLOT(resourcesReceived(RequestId, QnApiResourceResponsePtr)));
    connect(&sm, SIGNAL(eventsReceived(QnApiEventResponsePtr)), this, SLOT(eventsReceived(QnApiEventResponsePtr)));
}

Client::~Client()
{
}

void Client::run()
{
    // sm.openEventChannel();

    m_state = INITIAL;
    nextStep();
}

void Client::eventsReceived(QnApiEventResponsePtr events)
{
    int n = 0;
    foreach(const QnApiEventResponsePtr::Type::event_type& e, events->event())
    {
        qDebug() << "Event " << n++ << ": " << e.name().c_str();
    }
}

void Client::camerasReceived(RequestId requestId, QnApiCameraResponsePtr cameras)
{
    if (requestId == camerasRequestId)
    {
        qDebug("cameras received");

        int n = 0;
        foreach(const QnApiCameraResponsePtr::Type::camera_type& camera, cameras->camera())
        {
            qDebug() << "Camera " << n++ << ": " << camera.name().c_str();
        }

        m_state = GOT_CAMERAS;
        nextStep();
    } else if (requestId == addCameraRequestId)
    {
        qDebug("camera received");

        int n = 0;
        foreach(const QnApiCameraResponsePtr::Type::camera_type& camera, cameras->camera())
        {
            qDebug() << "Camera " << n++ << ": " << camera.name().c_str();
        }

        m_state = ADDED_CAMERA;
        nextStep();
    }
}

void Client::serverAdded(RequestId requestId, QnApiServerResponsePtr servers)
{
    if (requestId != addServerRequestId)
        return;

    if (servers->server().size() != 1)
    {
        qDebug("Error. Returned > 1 servers.");
        return;
    }

    const QnApiServerResponsePtr::Type::server_type& server = servers->server()[0];
    m_serverId = server.id();
    writeServerId(m_serverId);

    m_state = GOT_SERVER_ID;
    nextStep();
}

void Client::layoutsReceived(RequestId requestId, QnApiLayoutResponsePtr layouts)
{
    if (requestId != layoutsRequestId)
        return;

    foreach(const QnApiLayoutResponsePtr::Type::layout_type& layout, layouts->layout())
    {
        qDebug() << "Layoout: " << layout.name().c_str();
    }

    m_state = GOT_LAYOUTS;
    nextStep();
}

void Client::serversReceived(RequestId requestId, QnApiServerResponsePtr servers)
{
    if (requestId != serversRequestId)
        return;

    foreach (const QnApiServerResponsePtr::Type::server_type& server, servers->server())
    {
        qDebug() << "Server: " << server.name().c_str();
    }

    m_state = GOT_SERVERS;
    nextStep();
}

void Client::resourcesReceived(RequestId requestId, QnApiResourceResponsePtr resources)
{
    if (requestId != resourcesRequestId)
        return;

    foreach (const QnApiResourceResponsePtr::Type::resource_type& resource, resources->resource())
    {
        qDebug() << "Resource: " << resource.name().c_str();
    }

    m_state = GOT_RESOURCES;
    nextStep();
}

void Client::resourceTypesReceived(RequestId requestId, QnApiResourceTypeResponsePtr resourceTypes)
{
    if (requestId != resourceTypesRequestId)
        return;

    qDebug("resource types received");

    m_resourceTypes = resourceTypes;

    using xsd::api::resourceTypes::resourceTypes_t;

    int n = 0;
    for (resourceTypes_t::resourceType_const_iterator i (resourceTypes->resourceType ().begin ());
             i != resourceTypes->resourceType ().end ();
             ++i)
    {
        qDebug() << "ResourceType " << n++ << ": " << i->name().c_str();
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
        return;

        if (!m_serverId.isEmpty())
        {
            m_state = GOT_SERVER_ID;
            nextStep();
        } else
        {
            Server server;
            server.setType(QString((int)m_resourceTypes->resourceType()[0].id()));
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
        camera.setType(QString((int)m_resourceTypes->resourceType()[0].id()));

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
