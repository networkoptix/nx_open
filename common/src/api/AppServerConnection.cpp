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


void conn_detail::ReplyProcessor::finished(int status, const QByteArray &result, int handle)
{
    if (m_objectName == "server")
    {
        QnResourceList servers;
        QnApiServerResponsePtr xsdServers;

        QTextStream stream(result);

        QByteArray errorString;
        AppSessionManager::loadObjects(stream.device(), xsdServers, ::xsd::api::servers::servers, errorString);
        if (status == 0)
        {
            parseServers(servers, xsdServers->server(), m_resourceFactory);
        } else
        {
            errorString += SessionManager::formatNetworkError(status);
        }

        emit finished(handle, status, errorString, servers);
    } else if (m_objectName == "camera")
    {
        QnResourceList cameras;
        QnApiCameraResponsePtr xsdCameras;

        QTextStream stream(result);

        QByteArray errorString;
        AppSessionManager::loadObjects(stream.device(), xsdCameras, ::xsd::api::cameras::cameras, errorString);
        if (status == 0)
        {
            parseCameras(cameras, xsdCameras->camera(), m_resourceFactory);
        } else
        {
            errorString += SessionManager::formatNetworkError(status);
        }

        emit finished(handle, status, errorString, cameras);
    }
}

QnAppServerConnection::QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory)
    : m_sessionManager(new AppSessionManager(url)),
      m_resourceFactory(resourceFactory)
{
}

QnAppServerConnection::~QnAppServerConnection()
{
}

void QnAppServerConnection::testConnectionAsync(QObject* target, const char* slot)
{
    return m_sessionManager->testConnectionAsync(target, slot);
}

bool QnAppServerConnection::isConnected() const
{
    return true;
}

int QnAppServerConnection::getResourceTypes(QList<QnResourceTypePtr>& resourceTypes, QByteArray& errorString)
{
    QnApiResourceTypeResponsePtr xsdResourceTypes;

    int status = m_sessionManager->getResourceTypes(xsdResourceTypes, errorString);

    if (!xsdResourceTypes.isNull())
        parseResourceTypes(resourceTypes, xsdResourceTypes->resourceType());

    return status;
}

int QnAppServerConnection::getResources(QList<QnResourcePtr>& resources, QByteArray& errorString)
{
    QnApiResourceResponsePtr xsdResources;

    int status = m_sessionManager->getResources(xsdResources, errorString);

    if (!xsdResources.isNull())
    {
        parseCameras(resources, xsdResources->cameras().camera(), m_resourceFactory);
        parseServers(resources, xsdResources->servers().server(), m_serverFactory);
        parseLayouts(resources, xsdResources->layouts().layout(), m_resourceFactory);
        parseUsers(resources, xsdResources->users().user(), m_resourceFactory);
    }

    return status;
}

int QnAppServerConnection::registerServer(const QnVideoServer& serverIn, QnVideoServerList& servers, QByteArray& errorString)
{
    QnApiServerPtr server = unparseServer(serverIn);

    QnApiServerResponsePtr xsdServers;

    if (m_sessionManager->registerServer(*server, xsdServers, errorString) == 0)
    {
        parseServers(servers, xsdServers->server(), m_resourceFactory);

        return 0;
    }

    return 1;
}

int QnAppServerConnection::addCamera(const QnCameraResource& cameraIn, QList<QnResourcePtr>& cameras, QByteArray& errorString)
{
    QnApiCameraPtr camera = unparseCamera(cameraIn);

    QnApiCameraResponsePtr xsdCameras;

    if (m_sessionManager->addCamera(*camera, xsdCameras, errorString) == 0)
    {
        parseCameras(cameras, xsdCameras->camera(), m_resourceFactory);
        return 0;
    }

    return 1;
}

int QnAppServerConnection::saveAsync(const QnResource& resource, QObject* target, const char* slot)
{
    if (dynamic_cast<const QnVideoServer*>(&resource))
        return saveAsync(*dynamic_cast<const QnVideoServer*>(&resource), target, slot);

    if (dynamic_cast<const QnCameraResource*>(&resource))
        return saveAsync(*dynamic_cast<const QnCameraResource*>(&resource), target, slot);

    return 0;
}

int QnAppServerConnection::saveAsync(const QnVideoServer& serverIn, QObject* target, const char* slot)
{
    QnApiServerPtr server = unparseServer(serverIn);

    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, "server");
    QObject::connect(processor, SIGNAL(finished(int, int, const QByteArray&, const QnResourceList&)), target, slot);

    m_sessionManager->registerServerAsync(*server, processor, SLOT(finished(int, const QByteArray&, int)));

    return 0;
}

int QnAppServerConnection::saveAsync(const QnCameraResource& cameraIn, QObject* target, const char* slot)
{
    QnApiCameraPtr camera = unparseCamera(cameraIn);

    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, "camera");
    QObject::connect(processor, SIGNAL(finished(int, int, const QByteArray&, const QnResourceList&)), target, slot);

    m_sessionManager->addCameraAsync(*camera, processor, SLOT(finished(int, const QByteArray&, int)));

    return 0;
}

int QnAppServerConnection::addStorage(const QnStorage& storageIn, QByteArray& errorString)
{
    xsd::api::storages::Storage storage(storageIn.getId().toString().toStdString(),
                                         storageIn.getName().toStdString(),
                                         storageIn.getUrl().toStdString(),
                                         storageIn.getTypeId().toString().toStdString(),
                                         storageIn.getParentId().toString().toStdString(),
                                         storageIn.getSpaceLimit(),
                                         storageIn.getMaxStoreTime());

    return m_sessionManager->addStorage(storage, errorString);
}

int QnAppServerConnection::getCameras(QnSecurityCamResourceList& cameras, const QnId& mediaServerId, QByteArray& errorString)
{
    QnApiCameraResponsePtr xsdCameras;

    int status = m_sessionManager->getCameras(xsdCameras, mediaServerId, errorString);

    if (!xsdCameras.isNull())
    {
        parseCameras(cameras, xsdCameras->camera(), m_resourceFactory);
    }

    return status;
}

int QnAppServerConnection::getStorages(QnResourceList& storages, QByteArray& errorString)
{
    QnApiStorageResponsePtr xsdStorages;

    int status = m_sessionManager->getStorages(xsdStorages, errorString);

    if (!xsdStorages.isNull())
    {
        parseStorages(storages, xsdStorages->storage(), m_resourceFactory);
    }

    return status;
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

void QnAppServerConnectionFactory::setDefaultFactory(QnResourceFactory* resourceFactory)
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        //        QMutexLocker locker(&factory->m_mutex);
        factory->m_resourceFactory = resourceFactory;
    }
}

QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection(const QUrl& url)
{
    cl_log.log(QLatin1String("Creating connection to the application server ") + url.toString(), cl_logALWAYS);

    return QnAppServerConnectionPtr(new QnAppServerConnection(url, *(theAppServerConnectionFactory()->m_resourceFactory)));
}

QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection()
{
    return createConnection(defaultUrl());
}
