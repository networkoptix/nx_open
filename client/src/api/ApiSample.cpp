#include <QtCore/QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>

#include "ApiSample.h"
#include "core/resourcemanagment/resource_pool.h"
#include "core/resource/file_resource.h"
#include "core/resourcemanagment/asynch_seacher.h"

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

Client::Client(const QHostAddress& host, int port, const QAuthenticator& auth)
    : m_appServer(new QnAppServerConnection(host, port, auth, QnResourceDiscoveryManager::instance()))
{
    m_serverId = readServerId();
}

Client::~Client()
{
}

void Client::requestResourceTypes()
    {
    m_appServer->getResourceTypes(m_resourceTypes);

    qDebug("resource types received");

    int n = 0;
    foreach(const QnResourceTypePtr& resourceType, m_resourceTypes)
    {
        qnResTypePool->addResourceType(resourceType);

        qDebug() << "ResourceType " << n++ << ":\n"
                 << "Name: " << resourceType->getName() << "\n"
                 << "Parent IDs" << resourceType->allParentList() << "\n";

        int m = 0;
        foreach (const QnParamTypePtr& paramType, resourceType->paramTypeList())
        {
            qDebug() << "Param " << m++ << paramType->name << "\n";
        }
    }
}

void Client::ensureServerRegistered()
{
    if (m_serverId.toString().toInt() != 0)
    {
        return;
    }

    QnServer server("server");
    server.setTypeId(QnId("2"));

    QList<QnServerPtr> servers;
    m_appServer->addServer(server, servers);

        qDebug("server received");

        int n = 0;
        foreach(const QnResourcePtr& server, servers)
        {
            qDebug() << "Server " << n++ << ": " << server->getName();

            m_serverId = server->getId();
            writeServerId(m_serverId.toString());
        }

}

void Client::requestAllResources()
{
    QList<QnResourcePtr> resources;

    m_appServer->getResources(resources);

    qDebug("resources received");

    int n = 0;
    foreach(const QnResourcePtr& resource, resources)
    {
        qnResPool->addResource(resource);

        qDebug() << "Resource " << n++ << ": " << resource->getName();
    }
}

void Client::addCamera()
{
#if 0
    QnNetworkResource camera("CameraBlin");
    camera.setTypeId(QnId("3"));

    QList<QnNetworkResourcePtr> cameras;
    m_appServer->addCamera(camera, m_serverId, cameras);

    qDebug("camera received");

    int n = 0;
    foreach(const QnResourcePtr& camera, cameras)
    {
        qDebug() << "Camera " << n++ << ": " << camera->getName();
    }
#endif
}

void Client::run()
{
    requestResourceTypes();
    ensureServerRegistered();
    requestAllResources();
    addCamera();
}

#ifdef API_TEST_MAIN
int main(int argc, char *argv[])
{
    QHostAddress host("127.0.0.1");
    QAuthenticator auth;
    auth.setUser("ivan");
    auth.setPassword("225653");

    qDebug() << host;

    Client client(host, auth);
    client.run();
}
#endif
