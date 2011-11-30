#include "AppServerConnection.h"

#include "api/parsers/parse_cameras.h"
#include "api/parsers/parse_layouts.h"
#include "api/parsers/parse_users.h"
#include "api/parsers/parse_servers.h"
#include "api/parsers/parse_resource_types.h"
#include "api/parsers/parse_storages.h"

#include "api/Types.h"
#include "api/AppSessionManager.h"

QnAppServerConnection::QnAppServerConnection(const QHostAddress& host, int port, const QAuthenticator& auth, QnResourceFactory& resourceFactory)
    :m_sessionManager(new AppSessionManager(host, port, auth)),
      m_resourceFactory(resourceFactory)
{
}

bool QnAppServerConnection::isConnected() const
{
    return true;
}

QnAppServerConnection::~QnAppServerConnection()
{
}

int QnAppServerConnection::getResourceTypes(QList<QnResourceTypePtr>& resourceTypes)
{
    QnApiResourceTypeResponsePtr xsdResourceTypes;

    int status = m_sessionManager->getResourceTypes(xsdResourceTypes);

    if (!xsdResourceTypes.isNull())
        parseResourceTypes(resourceTypes, xsdResourceTypes->resourceType());

    return status;
}

int QnAppServerConnection::getResources(QList<QnResourcePtr>& resources)
{
    QnApiResourceResponsePtr xsdResources;

    int status = m_sessionManager->getResources(xsdResources);

    if (!xsdResources.isNull())
    {
        parseCameras(resources, xsdResources->cameras().camera(), m_resourceFactory);
        parseServers(resources, xsdResources->servers().server(), m_serverFactory);
        parseLayouts(resources, xsdResources->layouts().layout(), m_resourceFactory);
        parseUsers(resources, xsdResources->users().user(), m_resourceFactory);
    }

    return status;
}

int QnAppServerConnection::addServer(const QnVideoServer& serverIn, QnVideoServerList& servers)
{
    xsd::api::servers::Server server(serverIn.getId().toString().toStdString(),
                                     serverIn.getName().toStdString(),
                                     serverIn.getTypeId().toString().toStdString(),
                                     serverIn.getUrl().toStdString(),
                                     serverIn.getGuid().toStdString(),
                                     serverIn.getApiUrl().toStdString());

    QnApiServerResponsePtr xsdServers;

    if (m_sessionManager->addServer(server, xsdServers) == 0)
    {
        parseServers(servers, xsdServers->server(), m_resourceFactory);
        return 0;
    }

    return 1;
}

int QnAppServerConnection::addCamera(const QnNetworkResource& cameraIn, const QnId& serverIdIn, QList<QnResourcePtr>& cameras)
{
    xsd::api::cameras::Camera camera(cameraIn.getId().toString().toStdString(),
                                     cameraIn.getName().toStdString(),
                                     cameraIn.getTypeId().toString().toStdString(),
                                     cameraIn.getUrl().toStdString(),
                                     cameraIn.getMAC().toString().toStdString(),
                                     cameraIn.getAuth().user().toStdString(),
                                     cameraIn.getAuth().password().toStdString());

    camera.parentID(serverIdIn.toString().toStdString());

    QnApiCameraResponsePtr xsdCameras;

    if (m_sessionManager->addCamera(camera, xsdCameras) == 0)
    {
        parseCameras(cameras, xsdCameras->camera(), m_resourceFactory);
        return 0;
    }

    return 1;
}

int QnAppServerConnection::getServers(QnResourceList& servers)
{
    // todo: implement me
    return 0;
}

int QnAppServerConnection::getStorages(QnResourceList& storages)
{
    QnApiStorageResponsePtr xsdStorages;

    int status = m_sessionManager->getStorages(xsdStorages);

    if (!xsdStorages.isNull())
    {
        parseStorages(storages, xsdStorages->storage(), m_resourceFactory);
    }

    return status;
}

QString QnAppServerConnection::getLastError() const
{
    return m_sessionManager->getLastError();
}
