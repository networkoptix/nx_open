#include "app_server_connection.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include "utils/common/sleep.h"
#include "session_manager.h"
#include "utils/common/synctime.h"


void conn_detail::ReplyProcessor::finished(const QnHTTPRawResponse& response, int handle)
{
    const QByteArray &result = response.data;
    int status = response.status;
    QByteArray errorString = response.errorString;

    if (status != 0)
        errorString += "\n" + response.data;

    if (m_objectName == QLatin1String("server"))
    {
        QnMediaServerResourceList servers;

        if(status == 0) {
            try {
                m_serializer.deserializeServers(servers, result, m_resourceFactory);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, QnResourceList(servers), handle);
    } else if (m_objectName == QLatin1String("camera"))
    {
        QnVirtualCameraResourceList cameras;

        if(status == 0) {
            try {
                m_serializer.deserializeCameras(cameras, result, m_resourceFactory);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, QnResourceList(cameras), handle);
    } else if (m_objectName == QLatin1String("user"))
    {
        QnUserResourceList users;

        if(status == 0) {
            try {
                m_serializer.deserializeUsers(users, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, QnResourceList(users), handle);
    } else if (m_objectName == QLatin1String("layout"))
    {
        QnLayoutResourceList layouts;

        if(status == 0) {
            try {
                m_serializer.deserializeLayouts(layouts, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, QnResourceList(layouts), handle);
    } else if (m_objectName == QLatin1String("resource"))
    {
        QnResourceList resources;

        if(status == 0) {
            try {
                m_serializer.deserializeResources(resources, result, m_resourceFactory);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, resources, handle);
    } else if (m_objectName == QLatin1String("license"))
    {
        QnLicenseList licenses;

        if(status == 0) {
            try {
                m_serializer.deserializeLicenses(licenses, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedLicense(status, errorString, licenses, handle);
    } else if (m_objectName == QLatin1String("connect"))
    {
        QnConnectInfoPtr connectInfo(new QnConnectInfo());

        if(status == 0) {
            try {
                m_serializer.deserializeConnectInfo(connectInfo, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedConnect(status, errorString, connectInfo, handle);
    }
}

QnAppServerConnection::QnAppServerConnection(const QUrl &url, QnResourceFactory& resourceFactory, QnApiSerializer& serializer, const QString &guid,
        const QString& authKey)
    : m_url(url),
      m_resourceFactory(resourceFactory),
      m_serializer(serializer)
{
    m_requestParams.append(QnRequestParam("format", m_serializer.format()));
    m_requestParams.append(QnRequestParam("guid", guid));
    m_requestHeaders.append(QnRequestHeader(QLatin1String("X-NetworkOptix-AuthKey"), authKey));
}

int QnAppServerConnection::addObject(const QString& objectName, const QByteArray& data, QnHTTPRawResponse& response)
{
    int status = QnSessionManager::instance()->sendPostRequest(m_url, objectName, m_requestHeaders, m_requestParams, data, response);
    if (status != 0)
    {
        response.errorString += "\nSessionManager::sendPostRequest(): ";
        response.errorString += QnSessionManager::formatNetworkError(status) + response.data;
    }

    return status;
}

int QnAppServerConnection::getObjectsAsync(const QString& objectName, const QString& args, QObject* target, const char* slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    if (!args.isEmpty())
    {
        QStringList argsKvList = args.split(QLatin1Char('='));

        if (argsKvList.length() == 2)
            requestParams.append(QnRequestParam(argsKvList[0], argsKvList[1]));
    }

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, objectName, requestHeaders, requestParams, target, slot);
}

int QnAppServerConnection::addObjectAsync(const QString& objectName, const QByteArray& data, QObject* target, const char* slot)
{
    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, objectName, m_requestHeaders, m_requestParams, data, target, slot);
}

int QnAppServerConnection::deleteObjectAsync(const QString& objectName, int id, QObject* target, const char* slot)
{
    return QnSessionManager::instance()->sendAsyncDeleteRequest(m_url, objectName, id, target, slot);
}

int QnAppServerConnection::getObjects(const QString& objectName, const QString& args, QnHTTPRawResponse& response)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    if (!args.isEmpty())
    {
        QStringList argsKvList = args.split(QLatin1Char('='));

        if (argsKvList.length() == 2)
            requestParams.append(QnRequestParam(argsKvList[0], argsKvList[1]));
    }

    return QnSessionManager::instance()->sendGetRequest(m_url, objectName, requestHeaders, requestParams, response);
}

int QnAppServerConnection::connectAsync_i(const QnRequestHeaderList& headers, const QnRequestParamList& params, QObject* target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, QLatin1String("connect"));
    QObject::connect(processor, SIGNAL(finishedConnect(int, const QByteArray&, QnConnectInfoPtr, int)), target, slot);

    QByteArray data;
    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, QLatin1String("connect"), headers, params, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::testConnectionAsync(QObject* target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList params;
    params.append(QnRequestParam("ping", "1"));

    return connectAsync_i(requestHeaders, params, target, slot);
}

int QnAppServerConnection::connectAsync(QObject* target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList params;

    return connectAsync_i(requestHeaders, params, target, slot);
}

int QnAppServerConnection::connect(QnConnectInfoPtr &connectInfo, QByteArray& errorString)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList params;

    QnHTTPRawResponse response;
    int status = QnSessionManager::instance()->sendPostRequest(m_url, QLatin1String("connect"), requestHeaders, params, QByteArray(), response);
    if (status == 0)
    {
        try {
            m_serializer.deserializeConnectInfo(connectInfo, response.data);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
            status = -1;
        }
    } else {
        errorString = response.errorString;
    }
    
    return status;
}

QnAppServerConnection::~QnAppServerConnection()
{
}

int QnAppServerConnection::getResourceTypes(QnResourceTypeList& resourceTypes, QByteArray& errorString)
{
    QnHTTPRawResponse response;

    int status = getObjects(QLatin1String("resourceType"), QString(), response);
    if (status == 0)
    {
        try {
            m_serializer.deserializeResourceTypes(resourceTypes, response.data);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    }

    errorString = response.errorString;
    return status;
}

int QnAppServerConnection::getResources(QnResourceList& resources, QByteArray& errorString)
{
    QnHTTPRawResponse response;
    int status = getObjects(QLatin1String("resource"), QString(), response);

    if (status == 0)
    {
        errorString.clear();
        try {
            m_serializer.deserializeResources(resources, response.data, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    } else {
        errorString = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getResource(const QnId& id, QnResourcePtr& resource, QByteArray& errorString)
{
    QnResourceList resources;
    int status = getResources(QString(QLatin1String("id=%1")).arg(id.toString()), resources, errorString);
    if (status == 0) 
        resource = resources[0];
    return status;
}

int QnAppServerConnection::getResources(const QString& args, QnResourceList& resources, QByteArray& errorString)
{
    QnHTTPRawResponse response;

    int status = getObjects(QLatin1String("resource"), args, response);
    if (status == 0)
    {
        errorString.clear();
        try {
            m_serializer.deserializeResources(resources, response.data, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    } else {
        errorString = response.errorString;
    }
    
    return status;
}

int QnAppServerConnection::registerServer(const QnMediaServerResourcePtr& serverPtr, QnMediaServerResourceList& servers, QByteArray& authKey, QByteArray& errorString)
{
    QByteArray data;

    m_serializer.serialize(serverPtr, data);

    QnHTTPRawResponse response;
    int status = addObject(QLatin1String("server"), data, response);
    if (status == 0)
    {
        try {
            m_serializer.deserializeServers(servers, response.data, m_resourceFactory);

            foreach(QNetworkReply::RawHeaderPair rawHeader, response.headers) {
                if (rawHeader.first == "X-NetworkOptix-AuthKey") {
                    authKey = rawHeader.second;
                }
            }
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    } else {
        errorString = response.errorString;
    }
    return status;
}

int QnAppServerConnection::addCamera(const QnVirtualCameraResourcePtr& cameraPtr, QnVirtualCameraResourceList& cameras, QByteArray& errorString)
{
    QByteArray data;

    m_serializer.serialize(cameraPtr, data);

    QnHTTPRawResponse response;
    if (addObject(QLatin1String("camera"), data, response) == 0)
    {
        try {
            m_serializer.deserializeCameras(cameras, response.data, m_resourceFactory);
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
    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
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
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, QLatin1String("license"));
    QObject::connect(processor, SIGNAL(finishedLicense(int, const QByteArray&, const QnLicenseList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeLicense(license, data);

    return addObjectAsync(QLatin1String("license"), data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getResourcesAsync(const QString& args, const QString& objectName, QObject *target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, objectName);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    return getObjectsAsync(objectName, args, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getLicensesAsync(QObject *target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, QLatin1String("license"));
    QObject::connect(processor, SIGNAL(finished(int,QByteArray,QnLicenseList,int)), target, slot);

    return getObjectsAsync(QLatin1String("license"), QString(), processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnUserResourcePtr& userPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, QLatin1String("user"));
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(userPtr, data);

    return addObjectAsync(QLatin1String("user"), data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnMediaServerResourcePtr& serverPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, QLatin1String("server"));
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(serverPtr, data);

    return addObjectAsync(QLatin1String("server"), data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnVirtualCameraResourcePtr& cameraPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, QLatin1String("camera"));
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(cameraPtr, data);

    return addObjectAsync(QLatin1String("camera"), data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnLayoutResourcePtr& layout, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, QLatin1String("layout"));
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(layout, data);

    return addObjectAsync(QLatin1String("layout"), data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnLayoutResourceList& layouts, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, QLatin1String("layout"));
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeLayouts(layouts, data);

    return addObjectAsync(QLatin1String("layout"), data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnVirtualCameraResourceList& cameras, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, QLatin1String("camera"));
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeCameras(cameras, data);

    return addObjectAsync(QLatin1String("camera"), data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::addStorage(const QnStorageResourcePtr& storagePtr, QByteArray& errorString)
{
    QByteArray data;
    m_serializer.serialize(storagePtr, data);

    QnHTTPRawResponse response;
    int status = addObject(QLatin1String("storage"), data, response);
    if (status)
        errorString = response.errorString;

    return status;
}

int QnAppServerConnection::addCameraHistoryItem(const QnCameraHistoryItem &cameraHistoryItem, QByteArray &errorString)
{
    QByteArray data;
    m_serializer.serializeCameraServerItem(cameraHistoryItem, data);

    QnHTTPRawResponse response;
    int status = addObject(QLatin1String("cameraServerItem"), data, response);
    if (status)
        errorString = response.errorString;

    return status;
}

int QnAppServerConnection::addBusinessRule(const QnBusinessEventRule &businessRule, QByteArray &errorString)
{
    QByteArray data;
    m_serializer.serializeBusinessRule(businessRule, data);

    QnHTTPRawResponse reply;
    const int result = addObject(QLatin1String("businessRule"), data, reply);
    errorString = reply.errorString;
    return result;
}


int QnAppServerConnection::getServers(QnMediaServerResourceList &servers, QByteArray &errorString)
{
    QnHTTPRawResponse response;
    int status = getObjects(QLatin1String("server"), QString(), response);

    try {
        m_serializer.deserializeServers(servers, response.data, m_resourceFactory);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
    }

    errorString = response.errorString;
    return status;
}

int QnAppServerConnection::getCameras(QnVirtualCameraResourceList& cameras, QnId mediaServerId, QByteArray& errorString)
{
    QnHTTPRawResponse response;

    int status = getObjects(QLatin1String("camera"), QString(QLatin1String("parent_id=%1")).arg(mediaServerId.toString()), response);
    if (status == 0) 
    {
        try {
            m_serializer.deserializeCameras(cameras, response.data, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    } else {
        errorString = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getLayouts(QnLayoutResourceList& layouts, QByteArray& errorString)
{
    QnHTTPRawResponse response;

    int status = getObjects(QLatin1String("layout"), QString(), response);
    if (status == 0) {
        try {
            m_serializer.deserializeLayouts(layouts, response.data);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    } else {
        errorString = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getUsers(QnUserResourceList& users, QByteArray& errorString)
{
    QnHTTPRawResponse response;

    int status = getObjects(QLatin1String("user"), QString(), response);
    if (status == 0) {
        try {
            m_serializer.deserializeUsers(users, response.data);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    } else {
        errorString = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getLicenses(QnLicenseList &licenses, QByteArray &errorString)
{
    QnHTTPRawResponse response;

    int status = getObjects(QLatin1String("license"), QString(), response);
    if (status == 0) {
        try {
            m_serializer.deserializeLicenses(licenses, response.data);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    } else {
        errorString = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getCameraHistoryList(QnCameraHistoryList &cameraHistoryList, QByteArray &errorString)
{
    QnHTTPRawResponse response;

    int status = getObjects(QLatin1String("cameraServerItem"), QString(), response);
    if (status == 0) {
        try {
            m_serializer.deserializeCameraHistoryList(cameraHistoryList, response.data);
        } catch (const QnSerializeException& e) {
            errorString += e.errorString();
        }
    } else {
        errorString = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getBusinessRules(QnBusinessEventRules &businessRules, QByteArray &errorString)
{
    QnHTTPRawResponse response;
    int status = getObjects(QLatin1String("businessRule"), QString(), response);

    errorString = response.errorString;
    try {
        m_serializer.deserializeBusinessRules(businessRules, response.data);
    } catch (const QnSerializeException& e) {
        errorString += e.errorString();
    }

    return status;
}

Q_GLOBAL_STATIC(QnAppServerConnectionFactory, theAppServerConnectionFactory)

QString QnAppServerConnectionFactory::authKey()
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        return factory->m_authKey;
    }

    return QString();
}

QString QnAppServerConnectionFactory::clientGuid()
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        return factory->m_clientGuid;
    }

    return QString();
}

QUrl QnAppServerConnectionFactory::defaultUrl()
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        return factory->m_defaultUrl;
    }

    return QUrl();
}

void QnAppServerConnectionFactory::setAuthKey(const QString &authKey)
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        factory->m_authKey = authKey;
    }
}

void QnAppServerConnectionFactory::setClientGuid(const QString &guid)
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        factory->m_clientGuid = guid;
    }
}

void QnAppServerConnectionFactory::setDefaultUrl(const QUrl &url)
{
    Q_ASSERT_X(url.isValid(), "QnAppServerConnectionFactory::initialize()", "an invalid url was passed");
    Q_ASSERT_X(!url.isRelative(), "QnAppServerConnectionFactory::initialize()", "relative urls aren't supported");

    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        factory->m_defaultUrl = url;
    }
}

void QnAppServerConnectionFactory::setDefaultFactory(QnResourceFactory* resourceFactory)
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        factory->m_resourceFactory = resourceFactory;
    }
}

QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection(const QUrl& url)
{
    QUrl urlNoPassword (url);
    urlNoPassword.setPassword(QString());

    cl_log.log(QLatin1String("Creating connection to the Enterprise Controller ") + urlNoPassword.toString(), cl_logDEBUG2);

    return QnAppServerConnectionPtr(new QnAppServerConnection(url,
                                                              *(theAppServerConnectionFactory()->m_resourceFactory),
                                                               theAppServerConnectionFactory()->m_serializer,
                                                              theAppServerConnectionFactory()->m_clientGuid,
                                                              theAppServerConnectionFactory()->m_authKey));
}

QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection()
{
    return createConnection(defaultUrl());
}

bool initResourceTypes(QnAppServerConnectionPtr appServerConnection)
{
    QList<QnResourceTypePtr> resourceTypeList;

    QByteArray errorString;
    if (appServerConnection->getResourceTypes(resourceTypeList, errorString) != 0)
    {
        qWarning() << "Can't get resource types: " << errorString;
        return false;
    }

    qnResTypePool->replaceResourceTypeList(resourceTypeList);

    return true;
}

bool initCameraHistory(QnAppServerConnectionPtr appServerConnection)
{
    QnCameraHistoryList cameraHistoryList;
    QByteArray errorString;
    if (appServerConnection->getCameraHistoryList(cameraHistoryList, errorString) != 0)
    {
        qDebug() << "QnMain::run(): Can't get cameras history. Reason: " << errorString;
        return false;
    }
    foreach(QnCameraHistoryPtr history, cameraHistoryList)
        QnCameraHistoryPool::instance()->addCameraHistory(history);
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

int QnAppServerConnection::saveSync(const QnMediaServerResourcePtr &server, QByteArray &errorString)
{
    QByteArray authKey;
    QnMediaServerResourceList servers;
    return registerServer(server, servers, authKey, errorString);
}

int QnAppServerConnection::saveSync(const QnVirtualCameraResourcePtr &camera, QByteArray &errorString)
{
    QnVirtualCameraResourceList cameras;
    return addCamera(camera, cameras, errorString);
}

int QnAppServerConnection::deleteAsync(const QnMediaServerResourcePtr& server, QObject* target, const char* slot)
{
    return deleteObjectAsync(QLatin1String("server"), server->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnVirtualCameraResourcePtr& camera, QObject* target, const char* slot)
{
    return deleteObjectAsync(QLatin1String("camera"), camera->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnUserResourcePtr& user, QObject* target, const char* slot)
{
    return deleteObjectAsync(QLatin1String("user"), user->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnLayoutResourcePtr& layout, QObject* target, const char* slot)
{
    return deleteObjectAsync(QLatin1String("layout"), layout->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnResourcePtr& resource, QObject* target, const char* slot) {
    if(QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>()) {
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
    QnHTTPRawResponse response;
    int rez = QnSessionManager::instance()->sendGetRequest(m_url, QLatin1String("time"), response);
    if (rez != 0) {
        qWarning() << "Can't read time from Enterprise Controller" << response.errorString;
        return QDateTime::currentMSecsSinceEpoch();
    }

    return response.data.toLongLong();
}

int QnAppServerConnection::sendEmail(const QString& to, const QString& subject, const QString& message, QByteArray& errorString)
{
    QnRequestParamList requestParams(m_requestParams);

    QnHTTPRawResponse response;
    QByteArray body;
    m_serializer.serializeEmail(to, subject, message, body);
    int status = QnSessionManager::instance()->sendPostRequest(m_url, QLatin1String("email"), QnRequestHeaderList(), requestParams, body, response);
    errorString = response.errorString;
    if (status != 0) {
        qWarning() << "Can't send email " << errorString;
        return status;
    }

    if (response.status != 200) {
        //errorString = reply;
        return -1;
    }

    return 0;
}

int QnAppServerConnection::setResourceStatusAsync(const QnId &resourceId, QnResource::Status status, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("status", QString::number(status)));

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, QLatin1String("status"), requestHeaders, requestParams, "", target, slot);
}

int QnAppServerConnection::setResourceDisabledAsync(const QnId &resourceId, bool disabled, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("disabled", QString::number((int)disabled)));

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, QLatin1String("disabled"), requestHeaders, requestParams, "", target, slot);
}

int QnAppServerConnection::setResourcesStatusAsync(const QnResourceList &resources, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    int n = 1;
    foreach (const QnResourcePtr resource, resources)
    {
        requestParams.append(QnRequestParam(QString(QLatin1String("id%1")).arg(n), resource->getId().toString()));
        requestParams.append(QnRequestParam(QString(QLatin1String("status%1")).arg(n), QString::number(resource->getStatus())));

        n++;
    }

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, QLatin1String("status"), requestHeaders, requestParams, "", target, slot);
}

int QnAppServerConnection::setResourceStatus(const QnId &resourceId, QnResource::Status status, QByteArray &errorString)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("status", QString::number(status)));

    QnHTTPRawResponse response;
    int result = QnSessionManager::instance()->sendPostRequest(m_url, QLatin1String("status"), requestHeaders, requestParams, "",
        response);
    errorString = response.errorString;
    return result;
}

int QnAppServerConnection::setResourcesDisabledAsync(const QnResourceList &resources, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    int n = 1;
    foreach (const QnResourcePtr resource, resources)
    {
        requestParams.append(QnRequestParam(QString(QLatin1String("id%1")).arg(n), resource->getId().toString()));
        requestParams.append(QnRequestParam(QString(QLatin1String("disabled%1")).arg(n), QString::number((int)resource->isDisabled())));

        n++;
    }

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, QLatin1String("disabled"), requestHeaders, requestParams, "", target, slot);
}

void QnAppServerConnectionFactory::setDefaultMediaProxyPort(int port)
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        factory->m_defaultMediaProxyPort = port;
    }
}

void QnAppServerConnectionFactory::setCurrentVersion(const QString& version)
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        factory->m_currentVersion = version;
    }
}

int QnAppServerConnectionFactory::defaultMediaProxyPort()
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        return factory->m_defaultMediaProxyPort;
    }

    return 0;
}

QString QnAppServerConnectionFactory::currentVersion()
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        return factory->m_currentVersion;
    }

    return QLatin1String("");
}

QnResourceFactory* QnAppServerConnectionFactory::defaultFactory()
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        return factory->m_resourceFactory;
    }

    return 0;
}
