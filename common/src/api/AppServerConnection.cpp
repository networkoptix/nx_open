#include "AppServerConnection.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include "api/AppSessionManager.h"

#include "utils/common/sleep.h"

void conn_detail::ReplyProcessor::finished(int status, const QByteArray &result, int handle)
{
    if (m_objectName == "server")
    {
        QnVideoServerList servers;

        QByteArray errorString;

        if (status == 0)
        {
            try {
                m_serializer.deserializeServers(servers, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        } else
        {
            errorString += SessionManager::formatNetworkError(status);
        }


        QnResourceList resources;
        qCopy(servers.begin(), servers.end(), std::back_inserter(resources));
        emit finished(handle, status, errorString, resources);
    } else if (m_objectName == "camera")
    {
        QnCameraResourceList cameras;

        QByteArray errorString;
        if (status == 0)
        {
            try {
                m_serializer.deserializeCameras(cameras, result, m_resourceFactory);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        } else
        {
            errorString += SessionManager::formatNetworkError(status);
        }

        QnResourceList resources;
        qCopy(cameras.begin(), cameras.end(), std::back_inserter(resources));
        emit finished(handle, status, errorString, resources);
    } else if (m_objectName == "user")
    {
        QnUserResourceList users;

        QByteArray errorString;
        if (status == 0)
        {
            try {
                m_serializer.deserializeUsers(users, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        } else
        {
            errorString += SessionManager::formatNetworkError(status);
        }

        QnResourceList resources;
        qCopy(users.begin(), users.end(), std::back_inserter(resources));
        emit finished(handle, status, errorString, resources);
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

int QnAppServerConnection::getResourceTypes(QnResourceTypeList& resourceTypes, QByteArray& errorString)
{
    QByteArray data;
    int status = m_sessionManager->getObjects("resourceType", "", data, errorString);

    try {
        m_serializer.deserializeResourceTypes(resourceTypes, data);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
    }

    return status;
}

int QnAppServerConnection::getResources(QList<QnResourcePtr>& resources, QByteArray& errorString)
{
    QByteArray data;
    int status = m_sessionManager->getObjects("resourceEx", "", data, errorString);

    if (status == 0)
    {
        try {
            m_serializer.deserializeResources(resources, data, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    }

    return status;
}

int QnAppServerConnection::registerServer(const QnVideoServerPtr& serverPtr, QnVideoServerList& servers, QByteArray& errorString)
{
    QByteArray data;

    m_serializer.serialize(serverPtr, data);

    QByteArray replyData;
    int status = m_sessionManager->addObject("server", data, replyData, errorString);
    try {
        if (status == 0)
        {
            try {
                m_serializer.deserializeServers(servers, replyData);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
        return -1;
    }

    return status;
}

int QnAppServerConnection::addCamera(const QnCameraResourcePtr& cameraPtr, QnCameraResourceList& cameras, QByteArray& errorString)
{
    QByteArray data;

    m_serializer.serialize(cameraPtr, data);

    QByteArray replyData;
    if (m_sessionManager->addObject("camera", data, replyData, errorString) == 0)
    {
        try {
            m_serializer.deserializeCameras(cameras, replyData, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }

        return 0;
    }

    return 1;
}

int QnAppServerConnection::saveAsync(const QnResourcePtr& resourcePtr, QObject* target, const char* slot)
{
    if (resourcePtr.dynamicCast<QnVideoServer>())
        return saveAsync(resourcePtr.dynamicCast<QnVideoServer>(), target, slot);
    else if (resourcePtr.dynamicCast<QnCameraResource>())
        return saveAsync(resourcePtr.dynamicCast<QnCameraResource>(), target, slot);
    else if (resourcePtr.dynamicCast<QnUserResource>())
        return saveAsync(resourcePtr.dynamicCast<QnUserResource>(), target, slot);

    return 0;
}

int QnAppServerConnection::saveAsync(const QnUserResourcePtr& userPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, "user");
    QObject::connect(processor, SIGNAL(finished(int, int, const QByteArray&, const QnResourceList&)), target, slot);

    QByteArray data;
    m_serializer.serialize(userPtr, data);

    m_sessionManager->addObjectAsync("user", data, processor, SLOT(finished(int, const QByteArray&, int)));

    return 0;
}

int QnAppServerConnection::saveAsync(const QnVideoServerPtr& serverPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, "server");
    QObject::connect(processor, SIGNAL(finished(int, int, const QByteArray&, const QnResourceList&)), target, slot);

    QByteArray data;
    m_serializer.serialize(serverPtr, data);

    m_sessionManager->addObjectAsync("server", data, processor, SLOT(finished(int, const QByteArray&, int)));

    return 0;
}

int QnAppServerConnection::saveAsync(const QnCameraResourcePtr& cameraPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, "camera");
    QObject::connect(processor, SIGNAL(finished(int, int, const QByteArray&, const QnResourceList&)), target, slot);

    QByteArray data;
    m_serializer.serialize(cameraPtr, data);

    m_sessionManager->addObjectAsync("camera", data, processor, SLOT(finished(int, const QByteArray&, int)));

    return 0;
}

int QnAppServerConnection::addStorage(const QnStoragePtr& storagePtr, QByteArray& errorString)
{
    QByteArray data;
    m_serializer.serialize(storagePtr, data);

    QByteArray reply;
    return m_sessionManager->addObject("storage", data, reply, errorString);
}

int QnAppServerConnection::getCameras(QnCameraResourceList& cameras, QnId mediaServerId, QByteArray& errorString)
{
    QByteArray data;
    int status = m_sessionManager->getObjects("camera", mediaServerId.toString(), data, errorString);

    try {
        m_serializer.deserializeCameras(cameras, data, m_resourceFactory);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
    }

    return status;
}

int QnAppServerConnection::getStorages(QnStorageList& storages, QByteArray& errorString)
{
    QByteArray data;
    int status = m_sessionManager->getObjects("storage", "", data, errorString);

    try {
        m_serializer.deserializeStorages(storages, data, m_resourceFactory);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
    }

    return status;
}

int QnAppServerConnection::getLayouts(QnLayoutDataList& layouts, QByteArray& errorString)
{
    QByteArray data;

    int status = m_sessionManager->getObjects("layout", "", data, errorString);

    try {
        m_serializer.deserializeLayouts(layouts, data);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
    }

    return status;
}

int QnAppServerConnection::getUsers(QnUserResourceList& users, QByteArray& errorString)
{
    QByteArray data;

    int status = m_sessionManager->getObjects("user", "", data, errorString);

    try {
        m_serializer.deserializeUsers(users, data);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
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

bool initResourceTypes(QnAppServerConnectionPtr appServerConnection)
{
    if (!qnResTypePool->isEmpty())
        return true;

    QList<QnResourceTypePtr> resourceTypeList;

    QByteArray errorString;
    if (appServerConnection->getResourceTypes(resourceTypeList, errorString) != 0)
    {
        qDebug() << "Can't get resource types: " << errorString;
        return false;
    }

    qnResTypePool->addResourceTypeList(resourceTypeList);

    return true;
}

