#include <QtCore/QCoreApplication>
#include <QDebug>

#include "Main.h"
#include "SessionManager.h"

Client::Client(const QString& host, const QString& login, const QString& password)
    : sm(host, login, password)
{
    connect(&sm, SIGNAL(resourceTypesReceived(int, QList<ResourceType*>*)), this, SLOT(resourceTypesReceived(int, QList<ResourceType*>*)));
    connect(&sm, SIGNAL(camerasReceived(int, QList<Camera*>*)), this, SLOT(camerasReceived(int, QList<Camera*>*)));
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
    sm.openEventChannel();

    m_state = INITIAL;
    nextStep();
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
        camerasRequestId = sm.getCameras();
        break;
    case GOT_CAMERAS:
        Camera camera;
        camera.setName("Qt Camera");
        camera.setType(m_resourceTypes->at(0)->id());

        QString serverId = "2";
        addCameraRequestId = sm.addCamera(camera, serverId);
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
