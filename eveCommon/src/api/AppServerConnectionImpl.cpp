#include "AppServerConnectionImpl.h"

#include "api/parsers/parse_cameras.h"
#include "api/parsers/parse_layouts.h"
#include "api/parsers/parse_servers.h"
#include "api/parsers/parse_resource_types.h"

#include "api/Types.h"
#include "api/SessionManager.h"


QnAppServerAdapter* createServerAdapter(const QString& host, const QString& login, const QString& password)
{
    return new QnAppServerAdapterImpl(host, login, password);
}

QnAppServerAdapterImpl::QnAppServerAdapterImpl(const QString& host, const QString& login, const QString& password)
    : m_sessionManager(host, login, password)
{
    connect(&m_sessionManager, SIGNAL(resourceTypesReceived(RequestId, QnApiResourceTypeResponsePtr)),
            this, SLOT(xsdResourceTypesReceived(RequestId, QnApiResourceTypeResponsePtr)));
    connect(&m_sessionManager, SIGNAL(resourcesReceived(RequestId, QnApiResourceResponsePtr)),
            this, SLOT(xsdResourcesReceived(RequestId, QnApiResourceResponsePtr)));
    connect(&m_sessionManager, SIGNAL(camerasReceived(RequestId, QnApiCameraResponsePtr)),
            this, SLOT(xsdCamerasReceived(RequestId, QnApiCameraResponsePtr)));
    connect(&m_sessionManager, SIGNAL(serversReceived(RequestId, QnApiServerResponsePtr)),
            this, SLOT(xsdServersReceived(RequestId, QnApiServerResponsePtr)));

    connect(&m_sessionManager, SIGNAL(error(RequestId, QString)),
            this, SLOT(xsdError(RequestId, QString)));
}

RequestId QnAppServerAdapterImpl::getResourceTypes()
{
    return m_sessionManager.getResourceTypes();
}

RequestId QnAppServerAdapterImpl::getResources()
{
    return m_sessionManager.getResources();
}

RequestId QnAppServerAdapterImpl::addServer(const QnResource& serverIn)
{
    xsd::api::servers::Server server(serverIn.getId().toString().toStdString(), serverIn.getName().toStdString(), serverIn.getTypeId().toString().toStdString());

    return m_sessionManager.addServer(server);
}

RequestId QnAppServerAdapterImpl::addCamera(const QnResource& cameraIn, const QnId& serverIdIn)
{
    xsd::api::cameras::Camera camera(cameraIn.getId().toString().toStdString(),
                                     cameraIn.getName().toStdString(),
                                     cameraIn.getTypeId().toString().toStdString(),
                                     "1",
                                     "url",
                                     "mac",
                                     "login",
                                     "password",
                                     serverIdIn.toString().toInt());

    return m_sessionManager.addCamera(camera);
}

void QnAppServerAdapterImpl::xsdResourceTypesReceived(RequestId requestId, QnApiResourceTypeResponsePtr xsdResourceTypes)
{
    QList<QnResourceTypePtr> resourceTypes;

    parseResourceTypes(resourceTypes, xsdResourceTypes->resourceType());

    emit resourceTypesReceived(requestId, resourceTypes);
}

void QnAppServerAdapterImpl::xsdResourcesReceived(RequestId requestId, QnApiResourceResponsePtr xsdResources)
{
    QList<QnResourcePtr> resources;

    if (xsdResources->cameras().present())
        parseCameras(resources, (*xsdResources->cameras()).camera());

    if (xsdResources->layouts().present())
        parseLayouts(resources, (*xsdResources->layouts()).layout());

    emit resourcesReceived(requestId, resources);
}

void QnAppServerAdapterImpl::xsdCamerasReceived(RequestId requestId, QnApiCameraResponsePtr xsdCameras)
{
    QList<QnResourcePtr> cameras;

    parseCameras(cameras, xsdCameras->camera());

    emit camerasReceived(requestId, cameras);
}

void QnAppServerAdapterImpl::xsdLayoutsReceived(RequestId requestId, QnApiLayoutResponsePtr xsdLayouts)
{
    QList<QnResourcePtr> layouts;

    parseLayouts(layouts, xsdLayouts->layout());

    emit layoutsReceived(requestId, layouts);
}

void QnAppServerAdapterImpl::xsdServersReceived(RequestId requestId, QnApiServerResponsePtr xsdServers)
{
    QList<QnResourcePtr> servers;

    parseServers(servers, xsdServers->server());

    emit serversReceived(requestId, servers);
}

void QnAppServerAdapterImpl::xsdError(RequestId requestId, QString message)
{
    emit error(requestId, message);
}
