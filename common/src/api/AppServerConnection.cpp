#include "AppServerConnection.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include "api/parsers/parse_cameras.h"
#include "api/parsers/parse_layouts.h"
#include "api/parsers/parse_users.h"
#include "api/parsers/parse_servers.h"
#include "api/parsers/parse_resource_types.h"
#include "api/parsers/parse_storages.h"
#include "api/parsers/parse_schedule_tasks.h"

#include "api/Types.h"
#include "api/AppSessionManager.h"

QnAppServerConnection::QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory)
    : m_sessionManager(new AppSessionManager(url)),
      m_resourceFactory(resourceFactory)
{
}

QnAppServerConnection::~QnAppServerConnection()
{
}

bool QnAppServerConnection::isConnected() const
{
    return true;
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

int QnAppServerConnection::addStorage(const QnStorage& storageIn)
{
    xsd::api::storages::Storage storage(storageIn.getId().toString().toStdString(),
                                         storageIn.getName().toStdString(),
                                         storageIn.getUrl().toStdString(),
                                         storageIn.getTypeId().toString().toStdString(),
                                         storageIn.getParentId().toString().toStdString(),
                                         storageIn.getSpaceLimit(),
                                         storageIn.getMaxStoreTime());

    return m_sessionManager->addStorage(storage);
}

int QnAppServerConnection::getCameras(QnSequrityCamResourceList& cameras, const QnId& mediaServerId)
{
    QnApiCameraResponsePtr xsdCameras;

    int status = m_sessionManager->getCameras(xsdCameras, mediaServerId);

    if (!xsdCameras.isNull())
    {
        parseCameras(cameras, xsdCameras->camera(), m_resourceFactory);
    }

    return status;
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

int QnAppServerConnection::getScheduleTasks(QnScheduleTaskList& scheduleTasks, const QnId& mediaServerId)
{
    QnApiScheduleTaskResponsePtr xsdScheduleTasks;

    int status = m_sessionManager->getScheduleTasks(xsdScheduleTasks, mediaServerId);

    if (!xsdScheduleTasks.isNull())
    {
        parseScheduleTasks(scheduleTasks, xsdScheduleTasks->scheduleTask(), m_resourceFactory);
    }

    return status;
}

QString QnAppServerConnection::lastError() const
{
    return m_sessionManager->lastError();
}


Q_GLOBAL_STATIC(QnAppServerConnectionFactory, theAppServerConnectionFactory)

QUrl QnAppServerConnectionFactory::defaultUrl()
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
//        QMutexLocker locker(&factory->m_mutex);
        return factory->m_url;
    }

    return QUrl();
}

void QnAppServerConnectionFactory::setDefaultUrl(const QUrl &url)
{
    Q_ASSERT_X(url.isValid(), "QnAppServerConnectionFactory::initialize()", "an invalid url has passed");
    Q_ASSERT_X(!url.isRelative(), "QnAppServerConnectionFactory::initialize()", "relative urls aren't supported");

    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
//        QMutexLocker locker(&factory->m_mutex);
        factory->m_url = url;
    }
}

QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection(QnResourceFactory &resourceFactory)
{
    QUrl url = defaultUrl();

    cl_log.log(QLatin1String("Creating connection to the application server ") + url.toString(), cl_logALWAYS);

    return QnAppServerConnectionPtr(new QnAppServerConnection(url, resourceFactory));
}
