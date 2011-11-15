#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>

#include "QStdIStream.h"

#include "AppSessionManager.h"

AppSessionManager::AppSessionManager(const QHostAddress& host, int port, const QAuthenticator& auth)
    : SessionManager(host, port, auth)
{
}

int AppSessionManager::addServer(const ::xsd::api::servers::Server& server, QnApiServerResponsePtr& serversPtr)
{
    std::ostringstream os;

    ::xsd::api::servers::Servers servers;
    servers.server().push_back(server);
    ::xsd::api::servers::servers(os, servers);

    QByteArray reply;

    int status = addObject("server", os.str().c_str(), reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            serversPtr = QnApiServerResponsePtr(xsd::api::servers::servers (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }
    }

    return status;
}

int AppSessionManager::addCamera(const ::xsd::api::cameras::Camera& camera, QnApiCameraResponsePtr& camerasPtr)
{
    std::ostringstream os;

    ::xsd::api::cameras::Cameras cameras;
    cameras.camera().push_back(camera);
    ::xsd::api::cameras::cameras(os, cameras);

    QByteArray reply;

    int status = addObject("camera", os.str().c_str(), reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            camerasPtr = QnApiCameraResponsePtr(xsd::api::cameras::cameras (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }
    }

    return status;
}

int AppSessionManager::addObject(const QString& objectName, const QByteArray& body, QByteArray& reply)
{
    QTextStream stream(&reply);

    int status = m_httpClient.syncPost(QString("api/%1/").arg(objectName), body, stream.device());
    stream.readAll();

    if (status != 0)
    {
        m_lastError = formatNetworkError(status) + reply;
    }

    return status;
}

int AppSessionManager::getResourceTypes(QnApiResourceTypeResponsePtr& resourceTypes)
{
    QByteArray reply;

    int status = sendGetRequest("resourceType", reply);
    if(status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            resourceTypes = QnApiResourceTypeResponsePtr(xsd::api::resourceTypes::resourceTypes (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }
    }

    return status;
}

int AppSessionManager::getStorages(QnApiStorageResponsePtr& storages)
{
    QByteArray reply;

    int status = sendGetRequest("storage", reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            storages = QnApiStorageResponsePtr(xsd::api::storages::storages (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }
    }

    return status;
}

int AppSessionManager::getResources(QnApiResourceResponsePtr& resources)
{
    QByteArray reply;

    int status = sendGetRequest("resourceEx", reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            resources = QnApiResourceResponsePtr(xsd::api::resourcesEx::resourcesEx (is, xml_schema::flags::dont_validate).release());

            return 0;
        } catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }
    }

    return status;
}
