#include "AppServerConnection.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include "utils/common/sleep.h"
#include "SessionManager.h"

void conn_detail::ReplyProcessor::finished(int status, const QByteArray &result, const QByteArray &errorStringIn, int handle)
{
    QByteArray errorString = errorStringIn;

    if (m_objectName == "server")
    {
        QnVideoServerResourceList servers;

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
        emit finished(status, errorString, resources, handle);
    } else if (m_objectName == "camera")
    {
        QnVirtualCameraResourceList cameras;

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
        emit finished(status, errorString, resources, handle);
    } else if (m_objectName == "user")
    {
        QnUserResourceList users;

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
        emit finished(status, errorString, resources, handle);
    }
}

QnAppServerConnection::QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory)
    : m_url(url),
      m_resourceFactory(resourceFactory)
{
}

int QnAppServerConnection::addObject(const QString& objectName, const QByteArray& data, QByteArray& reply, QByteArray& errorString)
{
    int status = SessionManager::instance()->sendPostRequest(m_url, objectName, data, reply, errorString);
    if (status != 0)
    {
        errorString += "\nSessionManager::sendPostRequest(): ";
        errorString += SessionManager::formatNetworkError(status) + reply;
    }

    return status;
}

int QnAppServerConnection::addObjectAsync(const QString& objectName, const QByteArray& data, QObject* target, const char* slot)
{
    return SessionManager::instance()->sendAsyncPostRequest(m_url, objectName, data, target, slot);
}

int QnAppServerConnection::getObjects(const QString& objectName, const QString& args, QByteArray& data, QByteArray& errorString)
{
    QString request;

    if (args.isEmpty())
        request = objectName;
    else
        request = QString("%1/%2").arg(objectName).arg(args);

    return SessionManager::instance()->sendGetRequest(m_url, request, data, errorString);
}

int QnAppServerConnection::testConnectionAsync(QObject* target, const char *slot)
{
    return SessionManager::instance()->testConnectionAsync(m_url, target, slot);
}

QnAppServerConnection::~QnAppServerConnection()
{
}

int QnAppServerConnection::getResourceTypes(QnResourceTypeList& resourceTypes, QByteArray& errorString)
{
    QByteArray data;
    int status = getObjects("resourceType", "", data, errorString);

	if (status == 0)
	{
		try {
			m_serializer.deserializeResourceTypes(resourceTypes, data);
		} catch (const QnSerializeException& e) {
			errorString += e.errorString();
		}
	}

    return status;
}

int QnAppServerConnection::getResources(QList<QnResourcePtr>& resources, QByteArray& errorString)
{
    QByteArray data;
    int status = getObjects("resourceEx", "", data, errorString);

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

int QnAppServerConnection::registerServer(const QnVideoServerResourcePtr& serverPtr, QnVideoServerResourceList& servers, QByteArray& errorString)
{
    QByteArray data;

    m_serializer.serialize(serverPtr, data);

    QByteArray replyData;
    int status = addObject("server", data, replyData, errorString);
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

int QnAppServerConnection::addCamera(const QnVirtualCameraResourcePtr& cameraPtr, QnVirtualCameraResourceList& cameras, QByteArray& errorString)
{
    QByteArray data;

    m_serializer.serialize(cameraPtr, data);

    QByteArray replyData;
    if (addObject("camera", data, replyData, errorString) == 0)
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
    if (resourcePtr.dynamicCast<QnVideoServerResource>())
        return saveAsync(resourcePtr.dynamicCast<QnVideoServerResource>(), target, slot);
    else if (resourcePtr.dynamicCast<QnVirtualCameraResource>())
        return saveAsync(resourcePtr.dynamicCast<QnVirtualCameraResource>(), target, slot);
    else if (resourcePtr.dynamicCast<QnUserResource>())
        return saveAsync(resourcePtr.dynamicCast<QnUserResource>(), target, slot);

    return 0;
}

int QnAppServerConnection::saveAsync(const QnUserResourcePtr& userPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, "user");
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(userPtr, data);

    return addObjectAsync("user", data, processor, SLOT(finished(int, QByteArray, QByteArray, int)));
}

int QnAppServerConnection::saveAsync(const QnVideoServerResourcePtr& serverPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, "server");
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(serverPtr, data);

    return addObjectAsync("server", data, processor, SLOT(finished(int, QByteArray, QByteArray, int)));
}

int QnAppServerConnection::saveAsync(const QnVirtualCameraResourcePtr& cameraPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, "camera");
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(cameraPtr, data);

    return addObjectAsync("camera", data, processor, SLOT(finished(int, QByteArray, QByteArray, int)));
}

int QnAppServerConnection::addStorage(const QnStorageResourcePtr& storagePtr, QByteArray& errorString)
{
    QByteArray data;
    m_serializer.serialize(storagePtr, data);

    QByteArray reply;
    return addObject("storage", data, reply, errorString);
}

int QnAppServerConnection::getCameras(QnVirtualCameraResourceList& cameras, QnId mediaServerId, QByteArray& errorString)
{
    QByteArray data;
    int status = getObjects("camera", mediaServerId.toString(), data, errorString);

    try {
        m_serializer.deserializeCameras(cameras, data, m_resourceFactory);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
    }

    return status;
}

int QnAppServerConnection::getStorages(QnStorageResourceList& storages, QByteArray& errorString)
{
    QByteArray data;
    int status = getObjects("storage", "", data, errorString);

    try {
        m_serializer.deserializeStorages(storages, data, m_resourceFactory);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
    }

    return status;
}

int QnAppServerConnection::getLayouts(QnLayoutResourceList& layouts, QByteArray& errorString)
{
    QByteArray data;

    int status = getObjects("layout", "", data, errorString);

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

    int status = getObjects("user", "", data, errorString);

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
        return factory->m_defaultUrl;
    }

    return QUrl();
}

void QnAppServerConnectionFactory::setDefaultUrl(const QUrl &url)
{
    Q_ASSERT_X(url.isValid(), "QnAppServerConnectionFactory::initialize()", "an invalid url has passed");
    Q_ASSERT_X(!url.isRelative(), "QnAppServerConnectionFactory::initialize()", "relative urls aren't supported");

    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
//        QMutexLocker locker(&factory->m_mutex);
        factory->m_defaultUrl = url;
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
    cl_log.log(QLatin1String("Creating connection to the application server ") + url.toString(), cl_logDEBUG2);

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

int QnAppServerConnection::saveSync(const QnVideoServerResourcePtr &server, QByteArray &errorString)
{
    QnVideoServerResourceList servers;
    return registerServer(server, servers, errorString);
}

int QnAppServerConnection::saveSync(const QnVirtualCameraResourcePtr &camera, QByteArray &errorString)
{
    QnVirtualCameraResourceList cameras;
    return addCamera(camera, cameras, errorString);
}
