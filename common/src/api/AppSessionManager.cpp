#include "AppSessionManager.h"

#include <QtNetwork/QNetworkReply>

#include <QtXml/QDomDocument>
#include <QtXml/QXmlInputSource>

#include <algorithm>

#include "utils/network/synchttp.h"

AppSessionManager::AppSessionManager(const QUrl &url)
    : SessionManager(url)
{
}

int AppSessionManager::addServer(const ::xsd::api::servers::Server& server, QnApiServerResponsePtr& serversPtr)
{
    ::xsd::api::servers::Servers servers;
    servers.server().push_back(server);

    return addObjects("server", servers, serversPtr, ::xsd::api::servers::servers, ::xsd::api::servers::servers);
}

int AppSessionManager::addCamera(const ::xsd::api::cameras::Camera& camera, QnApiCameraResponsePtr& camerasPtr)
{
    ::xsd::api::cameras::Cameras cameras;
    cameras.camera().push_back(camera);

    return addObjects("camera", cameras, camerasPtr, ::xsd::api::cameras::cameras, ::xsd::api::cameras::cameras);
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
    return getObjects<xsd::api::resourceTypes::ResourceTypes>("resourceType", "", resourceTypes, xsd::api::resourceTypes::resourceTypes);
}

int AppSessionManager::getStorages(QnApiStorageResponsePtr& xstorages)
{
    return getObjects<xsd::api::storages::Storages>("storage", "", xstorages, xsd::api::storages::storages);
}

int AppSessionManager::getCameras(QnApiCameraResponsePtr& cameras, const QnId& mediaServerId)
{
    return getObjects<xsd::api::cameras::Cameras>("camera", mediaServerId.toString(), cameras, xsd::api::cameras::cameras);
}

int AppSessionManager::getResources(QnApiResourceResponsePtr& resources)
{
    return getObjects<xsd::api::resourcesEx::Resources>("resourceEx", "", resources, xsd::api::resourcesEx::resourcesEx);
}
