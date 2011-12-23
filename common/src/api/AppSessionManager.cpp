#include "AppSessionManager.h"

#include <QtNetwork/QNetworkReply>

#include <QtXml/QDomDocument>
#include <QtXml/QXmlInputSource>

#include "utils/network/synchttp.h"
#include "QStdIStream.h"

AppSessionManager::AppSessionManager(const QUrl &url)
    : SessionManager(url)
{
}

int AppSessionManager::addServer(const ::xsd::api::servers::Server& server, QnApiServerResponsePtr& serversPtr)
{
    std::ostringstream os;

    ::xsd::api::servers::Servers servers;
    servers.server().push_back(server);
    ::xsd::api::servers::servers(os, servers, ::xml_schema::namespace_infomap (), "UTF-8", XSD_FLAGS);

    QByteArray reply;

    int status = addObject(QLatin1String("server"), os.str().c_str(), reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            serversPtr = QnApiServerResponsePtr(xsd::api::servers::servers (is, XSD_FLAGS).release());

            return 0;
        }
        catch (const xml_schema::exception& e)
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
    ::xsd::api::cameras::cameras(os, cameras, ::xml_schema::namespace_infomap (), "UTF-8", XSD_FLAGS);

    QByteArray reply;

    int status = addObject(QLatin1String("camera"), os.str().c_str(), reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            camerasPtr = QnApiCameraResponsePtr(xsd::api::cameras::cameras (is, XSD_FLAGS).release());

            return 0;
        }
        catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }
    }

    return status;
}

int AppSessionManager::addStorage(const ::xsd::api::storages::Storage& storage)
{
    std::ostringstream os;

    ::xsd::api::storages::Storages storages;
    storages.storage().push_back(storage);
    ::xsd::api::storages::storages(os, storages, ::xml_schema::namespace_infomap (), "UTF-8", XSD_FLAGS);

    QByteArray reply;

    return addObject(QLatin1String("storage"), os.str().c_str(), reply);
}

int AppSessionManager::addObject(const QString& objectName, const QByteArray& body, QByteArray& reply)
{
    QTextStream stream(&reply);

    int status = m_httpClient->syncPost(createApiUrl(objectName), body, stream.device());
    stream.readAll();

    if (status != 0)
        m_lastError = formatNetworkError(status) + reply;

    return status;
}

int AppSessionManager::getResourceTypes(QnApiResourceTypeResponsePtr& resourceTypes)
{
    QByteArray reply;

    int status = sendGetRequest(QLatin1String("resourceType"), reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            resourceTypes = QnApiResourceTypeResponsePtr(xsd::api::resourceTypes::resourceTypes (is, XSD_FLAGS).release());

            return 0;
        }
        catch (const xml_schema::exception& e)
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

    int status = sendGetRequest(QLatin1String("storage"), reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            storages = QnApiStorageResponsePtr(xsd::api::storages::storages (is, XSD_FLAGS).release());

            return 0;
        }
        catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }
    }

    return status;
}

int AppSessionManager::getCameras(QnApiCameraResponsePtr& scheduleTasks, const QnId& mediaServerId)
{
    QByteArray reply;

    int status = sendGetRequest(QString("camera/%1").arg(mediaServerId.toString()), reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            scheduleTasks = QnApiCameraResponsePtr(xsd::api::cameras::cameras (is, XSD_FLAGS).release());

            return 0;
        }
        catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }
    }

    return status;
}

int AppSessionManager::getScheduleTasks(QnApiScheduleTaskResponsePtr& scheduleTasks, const QnId& mediaServerId)
{
    QByteArray reply;

    int status = sendGetRequest(QString("scheduleTask/%1").arg(mediaServerId.toString()), reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            scheduleTasks = QnApiScheduleTaskResponsePtr(xsd::api::scheduleTasks::scheduleTasks (is, XSD_FLAGS).release());

            return 0;
        }
        catch (const xml_schema::exception& e)
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

    int status = sendGetRequest(QLatin1String("resourceEx"), reply);
    if (status == 0)
    {
        try
        {
            QTextStream stream(reply);
            QStdIStream is(stream.device());

            resources = QnApiResourceResponsePtr(xsd::api::resourcesEx::resourcesEx (is, XSD_FLAGS).release());

            return 0;
        }
        catch (const xml_schema::exception& e)
        {
            m_lastError = e.what();

            qDebug(e.what());
            return -1;
        }
    }

    return status;
}
