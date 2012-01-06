#include "AppSessionManager.h"

#include <QtNetwork/QNetworkReply>

#include <QtXml/QDomDocument>
#include <QtXml/QXmlInputSource>

#include "utils/network/synchttp.h"

AppSessionManager::AppSessionManager(const QUrl &url)
    : SessionManager(url)
{
}

int AppSessionManager::addServer(const ::xsd::api::servers::Server& server, QnApiServerResponsePtr& serversPtr, QByteArray& errorString)
{
    ::xsd::api::servers::Servers servers;
    servers.server().push_back(server);

    return addObjects("server", servers, serversPtr, ::xsd::api::servers::servers, ::xsd::api::servers::servers, errorString);
}

int AppSessionManager::addCamera(const ::xsd::api::cameras::Camera& camera, QnApiCameraResponsePtr& camerasPtr, QByteArray& errorString)
{
    ::xsd::api::cameras::Cameras cameras;
    cameras.camera().push_back(camera);

    return addObjects("camera", cameras, camerasPtr, ::xsd::api::cameras::cameras, ::xsd::api::cameras::cameras, errorString);
}

int AppSessionManager::addServerAsync(const ::xsd::api::servers::Server& server, QObject* target, const char* slot)
{
    ::xsd::api::servers::Servers servers;
    servers.server().push_back(server);

    return addObjectsAsync("server", servers, ::xsd::api::servers::servers, target, slot);
}

int AppSessionManager::addCameraAsync(const ::xsd::api::cameras::Camera& camera, QObject* target, const char* slot)
{
    ::xsd::api::cameras::Cameras cameras;
    cameras.camera().push_back(camera);

    return addObjectsAsync("camera", cameras, ::xsd::api::cameras::cameras, target, slot);
}

int AppSessionManager::addStorage(const ::xsd::api::storages::Storage& storage, QByteArray& errorString)
{
    std::ostringstream os;

    ::xsd::api::storages::Storages storages;
    storages.storage().push_back(storage);
    ::xsd::api::storages::storages(os, storages, ::xml_schema::namespace_infomap (), "UTF-8", XSD_FLAGS);

    QByteArray reply;

    return addObject(QLatin1String("storage"), os.str().c_str(), reply, errorString);
}

int AppSessionManager::addObject(const QString& objectName, const QByteArray& body, QByteArray& reply, QByteArray& errorString)
{
    QTextStream stream(&reply);

    int status = m_httpClient->syncPost(createApiUrl(objectName), body, stream.device());
    stream.readAll();

    if (status != 0)
    {
        errorString += "\nAppSessionManager::addObject(): ";
        errorString += formatNetworkError(status) + reply;
    }

    return status;
}

int AppSessionManager::getResourceTypes(QnApiResourceTypeResponsePtr& resourceTypes, QByteArray& errorString)
{
    return getObjects<xsd::api::resourceTypes::ResourceTypes>("resourceType", "", resourceTypes, xsd::api::resourceTypes::resourceTypes, errorString);
}

int AppSessionManager::getStorages(QnApiStorageResponsePtr& xstorages, QByteArray& errorString)
{
    return getObjects<xsd::api::storages::Storages>("storage", "", xstorages, xsd::api::storages::storages, errorString);
}

int AppSessionManager::getCameras(QnApiCameraResponsePtr& cameras, const QnId& mediaServerId, QByteArray& errorString)
{
    return getObjects<xsd::api::cameras::Cameras>("camera", mediaServerId.toString(), cameras, xsd::api::cameras::cameras, errorString);
}

int AppSessionManager::getResources(QnApiResourceResponsePtr& resources, QByteArray& errorString)
{
    return getObjects<xsd::api::resourcesEx::Resources>("resourceEx", "", resources, xsd::api::resourcesEx::resourcesEx, errorString);
}
