#include "AppServerConnection.h"

#include "api/parsers/parse_cameras.h"
#include "api/parsers/parse_layouts.h"
#include "api/parsers/parse_users.h"
#include "api/parsers/parse_servers.h"
#include "api/parsers/parse_resource_types.h"

#include "api/Types.h"
#include "api/AppSessionManager.h"

QnAppServerConnection::QnAppServerConnection(const QHostAddress& host, int port, const QAuthenticator& auth, QnResourceFactory& resourceFactory)
    :m_sessionManager(new AppSessionManager(host, port, auth)),
      m_resourceFactory(resourceFactory)
{
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
        parseLayouts(resources, xsdResources->layouts().layout(), m_resourceFactory);
        parseUsers(resources, xsdResources->users().user(), m_resourceFactory);
    }

    return status;
}

int QnAppServerConnection::addServer(const QnServer& serverIn, QList<QnServerPtr>& servers)
{
    xsd::api::servers::Server server(serverIn.getId().toString().toStdString(),
                                     serverIn.getName().toStdString(),
                                     serverIn.getTypeId().toString().toStdString(),
                                     serverIn.getUrl().toStdString(),
                                     serverIn.getMAC().toString().toStdString());

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
                                     "1",
                                     cameraIn.getUrl().toStdString(),
                                     cameraIn.getMAC().toString().toStdString(),
                                     cameraIn.getAuth().user().toStdString(),
                                     cameraIn.getAuth().password().toStdString());

    camera.parentID(serverIdIn.toString().toInt());

    QnApiCameraResponsePtr xsdCameras;

    if (m_sessionManager->addCamera(camera, xsdCameras) == 0)
    {
        parseCameras(cameras, xsdCameras->camera(), m_resourceFactory);
        return 0;
    }

    return 1;
}
