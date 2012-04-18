#include "AppServerConnection.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include "utils/common/sleep.h"
#include "SessionManager.h"
#include "utils/common/synctime.h"

void conn_detail::ReplyProcessor::finished(int status, const QByteArray &result, const QByteArray &errorStringIn, int handle)
{
    QByteArray errorString = errorStringIn;

    if (status != 0)
    {
        errorString += "\n" + result;
        QnResourceList resources;
        emit finished(status, errorString, resources, handle);
        return;
    }

    if (m_objectName == "server")
    {
        QnVideoServerResourceList servers;

        try {
            m_serializer.deserializeServers(servers, result);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }

        QnResourceList resources;
        qCopy(servers.begin(), servers.end(), std::back_inserter(resources));
        emit finished(status, errorString, resources, handle);
    } else if (m_objectName == "camera")
    {
        QnVirtualCameraResourceList cameras;

        try {
            m_serializer.deserializeCameras(cameras, result, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }

        QnResourceList resources;
        qCopy(cameras.begin(), cameras.end(), std::back_inserter(resources));
        emit finished(status, errorString, resources, handle);
    } else if (m_objectName == "user")
    {
        QnUserResourceList users;

        try {
            m_serializer.deserializeUsers(users, result);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }

        QnResourceList resources;
        qCopy(users.begin(), users.end(), std::back_inserter(resources));
        emit finished(status, errorString, resources, handle);
    } else if (m_objectName == "layout")
    {
        QnLayoutResourceList layouts;

        try {
            m_serializer.deserializeLayouts(layouts, result);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }

        QnResourceList resources;
        qCopy(layouts.begin(), layouts.end(), std::back_inserter(resources));
        emit finished(status, errorString, resources, handle);
    }
}

void conn_detail::LicenseReplyProcessor::finished(int status, const QByteArray &result, const QByteArray &errorStringIn, int handle)
{
    QByteArray errorString = errorStringIn;

    QnLicenseList licenses;

    if (status == 0)
    {
        try {
            m_serializer.deserializeLicenses(licenses, result);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    } else
    {
        errorString += SessionManager::formatNetworkError(status);
    }

    emit finished(status, errorString, licenses, handle);
}

QnAppServerConnection::QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory, QnApiSerializer& serializer)
    : m_url(url),
      m_resourceFactory(resourceFactory),
      m_serializer(serializer)
{
    m_requestParams.append(QnRequestParam("format", m_serializer.format()));
}

int QnAppServerConnection::addObject(const QString& objectName, const QByteArray& data, QByteArray& reply, QByteArray& errorString)
{
    int status = SessionManager::instance()->sendPostRequest(m_url, objectName, m_requestParams, data, reply, errorString);
    if (status != 0)
    {
        errorString += "\nSessionManager::sendPostRequest(): ";
        errorString += SessionManager::formatNetworkError(status) + reply;
    }

    return status;
}

int QnAppServerConnection::getObjectsAsync(const QString& objectName, const QString& args, QObject* target, const char* slot)
{
    QnRequestParamList requestParams(m_requestParams);

    if (!args.isEmpty())
    {
        QStringList argsKvList = args.split("=");

        if (argsKvList.length() == 2)
            requestParams.append(QnRequestParam(argsKvList[0], argsKvList[1]));
    }

    return SessionManager::instance()->sendAsyncGetRequest(m_url, objectName, requestParams, target, slot);
}

int QnAppServerConnection::addObjectAsync(const QString& objectName, const QByteArray& data, QObject* target, const char* slot)
{
    return SessionManager::instance()->sendAsyncPostRequest(m_url, objectName, m_requestParams, data, target, slot);
}

int QnAppServerConnection::deleteObjectAsync(const QString& objectName, int id, QObject* target, const char* slot)
{
    return SessionManager::instance()->sendAsyncDeleteRequest(m_url, objectName, id, target, slot);
}

int QnAppServerConnection::getObjects(const QString& objectName, const QString& args, QByteArray& data, QByteArray& errorString)
{
    QnRequestParamList requestParams(m_requestParams);

    if (!args.isEmpty())
    {
        QStringList argsKvList = args.split("=");

        if (argsKvList.length() == 2)
            requestParams.append(QnRequestParam(argsKvList[0], argsKvList[1]));
    }

    return SessionManager::instance()->sendGetRequest(m_url, objectName, requestParams, data, errorString);
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

int QnAppServerConnection::getResources(QnResourceList& resources, QByteArray& errorString)
{
    QByteArray data;
    int status = getObjects("resourceEx", "", data, errorString);

    if (status == 0)
    {
        errorString.clear();
        try {
            m_serializer.deserializeResources(resources, data, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    }

    return status;
}

int QnAppServerConnection::getResource(const QnId& id, QnResourcePtr& resource, QByteArray& errorString)
{
    QnResourceList resources;
    int status = getResources(QString("id=%1").arg(id.toString()), resources, errorString);
    if (status == 0) 
        resource = resources[0];
    return status;
}

int QnAppServerConnection::getResources(const QString& args, QnResourceList& resources, QByteArray& errorString)
{
    QByteArray data;
    int status = getObjects("resourceEx", args, data, errorString);

    if (status == 0)
    {
        errorString.clear();
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
            cl_log.log(cl_logERROR, "AHTUNG! the client and app server version must be unsynched!");
            return 1;
        }

        return 0;
    }

    return 1;
}

int QnAppServerConnection::saveAsync(const QnResourcePtr& resource, QObject* target, const char* slot)
{
    if (QnVideoServerResourcePtr server = resource.dynamicCast<QnVideoServerResource>())
        return saveAsync(server, target, slot);
    else if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        return saveAsync(camera, target, slot);
    else if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
        return saveAsync(user, target, slot);
    else if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
        return saveAsync(layout, target, slot);

    return 0;
}

int QnAppServerConnection::addLicenseAsync(const QnLicensePtr &license, QObject *target, const char *slot)
{
    conn_detail::LicenseReplyProcessor* processor = new conn_detail::LicenseReplyProcessor(m_serializer, "license");
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnLicenseList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeLicense(license, data);

    return addObjectAsync("license", data, processor, SLOT(finished(int, QByteArray, QByteArray, int)));
}

int QnAppServerConnection::getResourcesAsync(const QString& args, const QString& objectName, QObject *target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, objectName);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    return getObjectsAsync(objectName, args, processor, SLOT(finished(int, QByteArray, QByteArray, int)));
}

int QnAppServerConnection::getLicensesAsync(QObject *target, const char *slot)
{
    conn_detail::LicenseReplyProcessor* processor = new conn_detail::LicenseReplyProcessor(m_serializer, "license");
    QObject::connect(processor, SIGNAL(finished(int,QByteArray,QnLicenseList,int)), target, slot);

    return getObjectsAsync("license", "", processor, SLOT(finished(int, QByteArray, QByteArray, int)));
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

int QnAppServerConnection::saveAsync(const QnLayoutResourcePtr& layout, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, "layout");
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(layout, data);

    return addObjectAsync("layout", data, processor, SLOT(finished(int, QByteArray, QByteArray, int)));
}

int QnAppServerConnection::saveAsync(const QnLayoutResourceList& layouts, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, "layout");
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeLayouts(layouts, data);

    return addObjectAsync("layout", data, processor, SLOT(finished(int, QByteArray, QByteArray, int)));
}

int QnAppServerConnection::saveAsync(const QnVirtualCameraResourceList& cameras, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, "camera");
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeCameras(cameras, data);

    return addObjectAsync("camera", data, processor, SLOT(finished(int, QByteArray, QByteArray, int)));
}

int QnAppServerConnection::addStorage(const QnStorageResourcePtr& storagePtr, QByteArray& errorString)
{
    QByteArray data;
    m_serializer.serialize(storagePtr, data);

    QByteArray reply;
    return addObject("storage", data, reply, errorString);
}

int QnAppServerConnection::getServers(QnVideoServerResourceList &servers, QByteArray &errorString)
{
    QByteArray data;
    int status = getObjects("server", "", data, errorString);

    try {
        m_serializer.deserializeServers(servers, data);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
    }

    return status;
}

int QnAppServerConnection::getCameras(QnVirtualCameraResourceList& cameras, QnId mediaServerId, QByteArray& errorString)
{
    QByteArray data;
    int status = getObjects("camera", QString("parent_id=%1").arg(mediaServerId.toString()), data, errorString);

    try {
        m_serializer.deserializeCameras(cameras, data, m_resourceFactory);
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

int QnAppServerConnection::getLicenses(QnLicenseList &licenses, QByteArray &errorString)
{
    QByteArray data;

    int status = getObjects("license", "", data, errorString);

    try {
        m_serializer.deserializeLicenses(licenses, data);
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
    cl_log.log(QLatin1String("Creating connection to the Enterprise Controller ") + url.toString(), cl_logDEBUG2);

    return QnAppServerConnectionPtr(new QnAppServerConnection(url,
                                                              *(theAppServerConnectionFactory()->m_resourceFactory),
                                                               theAppServerConnectionFactory()->m_serializer));
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
        qWarning() << "Can't get resource types: " << errorString;
        return false;
    }

    qnResTypePool->addResourceTypeList(resourceTypeList);

    return true;
}

bool initLicenses(QnAppServerConnectionPtr appServerConnection)
{
    if (!qnLicensePool->isEmpty())
        return true;

    QnLicenseList licenses;

    QByteArray errorString;
    if (appServerConnection->getLicenses(licenses, errorString) != 0)
    {
        qDebug() << "Can't get licenses: " << errorString;
        return false;
    }

    foreach (QnLicensePtr license, licenses.licenses())
    {
        // If some license is invalid set hardwareId to some invalid value and clear licenses
        if (!license->isValid())
        {
            // Returning true to stop asking for licenses

            QnLicenseList dummy;
            dummy.setHardwareId("invalid");
            qnLicensePool->replaceLicenses(dummy);

            return true;
        }
    }

    qnLicensePool->replaceLicenses(licenses);

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

int QnAppServerConnection::deleteAsync(const QnVideoServerResourcePtr& server, QObject* target, const char* slot)
{
    return deleteObjectAsync("server", server->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnVirtualCameraResourcePtr& camera, QObject* target, const char* slot)
{
    return deleteObjectAsync("camera", camera->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnUserResourcePtr& user, QObject* target, const char* slot)
{
    return deleteObjectAsync("user", user->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnLayoutResourcePtr& layout, QObject* target, const char* slot)
{
    return deleteObjectAsync("layout", layout->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnResourcePtr& resource, QObject* target, const char* slot) {
    if(QnVideoServerResourcePtr server = resource.dynamicCast<QnVideoServerResource>()) {
        return deleteAsync(server, target, slot);
    } else if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>()) {
        return deleteAsync(camera, target, slot);
    } else if(QnUserResourcePtr user = resource.dynamicCast<QnUserResource>()) {
        return deleteAsync(user, target, slot);
    } else if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
        return deleteAsync(layout, target, slot);
    } else {
        qnWarning("Cannot delete resources of type '%1'.", resource->metaObject()->className());
        return 0;
    }
}

qint64 QnAppServerConnection::getCurrentTime()
{
    QByteArray data;
    QByteArray errorString;

    int rez = SessionManager::instance()->sendGetRequest(m_url, "time", data, errorString);
    if (rez != 0) {
        qWarning() << "Can't read time from Enterprise Controller" << errorString;
        return QDateTime::currentMSecsSinceEpoch();
    }

    return data.toLongLong();
}

int QnAppServerConnection::setResourceStatusAsync(const QnId &resourceId, QnResource::Status status, QObject *target, const char *slot)
{
    QnRequestParamList requestParams;
    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("status", QString::number(status)));

    return SessionManager::instance()->sendAsyncPostRequest(m_url, "status", requestParams, "", target, slot);
}

int QnAppServerConnection::setResourceDisabledAsync(const QnId &resourceId, bool disabled, QObject *target, const char *slot)
{
    QnRequestParamList requestParams;
    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("disabled", QString::number((int)disabled)));

    return SessionManager::instance()->sendAsyncPostRequest(m_url, "disabled", requestParams, "", target, slot);
}

int QnAppServerConnection::setResourcesStatusAsync(const QnResourceList &resources, QObject *target, const char *slot)
{
    QnRequestParamList requestParams;

    int n = 1;
    foreach (const QnResourcePtr resource, resources)
    {
        requestParams.append(QnRequestParam(QString("id%1").arg(n), resource->getId().toString()));
        requestParams.append(QnRequestParam(QString("status%1").arg(n), QString::number(resource->getStatus())));

        n++;
    }

    return SessionManager::instance()->sendAsyncPostRequest(m_url, "status", requestParams, "", target, slot);
}

int QnAppServerConnection::setResourcesDisabledAsync(const QnResourceList &resources, QObject *target, const char *slot)
{
    QnRequestParamList requestParams;

    int n = 1;
    foreach (const QnResourcePtr resource, resources)
    {
        requestParams.append(QnRequestParam(QString("id%1").arg(n), resource->getId().toString()));
        requestParams.append(QnRequestParam(QString("disabled%1").arg(n), QString::number((int)resource->isDisabled())));

        n++;
    }

    return SessionManager::instance()->sendAsyncPostRequest(m_url, "disabled", requestParams, "", target, slot);
}

