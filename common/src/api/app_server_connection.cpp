#include "app_server_connection.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include "utils/common/sleep.h"
#include "session_manager.h"
#include "utils/common/synctime.h"
#include "message.pb.h"

#include <business/actions/abstract_business_action.h>

namespace {
    const QLatin1String cameraObject("camera");
    const QLatin1String resourceObject("resource");
    const QLatin1String serverObject("server");
    const QLatin1String userObject("user");
    const QLatin1String layoutObject("layout");
    const QLatin1String licenseObject("license");
    const QLatin1String businessRuleObject("businessRule");
    const QLatin1String connectObject("connect");
    const QLatin1String timeObject("time");
    const QLatin1String emailObject("email");
    const QLatin1String statusObject("status");
    const QLatin1String disabledObject("disabled");
    const QLatin1String panicObject("panic");
    const QLatin1String bbaObject("businessAction");
    const QLatin1String kvPairObject("kvPair");
    const QLatin1String dumpdbObject("dumpdb");
    const QLatin1String restoredbObject("restoredb");
    const QLatin1String settingObject("setting");
}

void conn_detail::ReplyProcessor::finished(const QnHTTPRawResponse& response, int handle)
{
    const QByteArray &result = response.data;
    int status = response.status;
    QByteArray errorString = response.errorString;

    if (status != 0)
        errorString += "\n" + response.data;

    if (m_objectName == serverObject)
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
    } else if (m_objectName == cameraObject)
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
    } else if (m_objectName == userObject)
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
    } else if (m_objectName == layoutObject)
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
    } else if (m_objectName == resourceObject)
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
    } else if (m_objectName == licenseObject)
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
    } else if (m_objectName ==connectObject)
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
    } else if (m_objectName == kvPairObject)
    {
        QnKvPairList kvPairs;

        if(status == 0) {
            try {
                m_serializer.deserializeKvPairs(kvPairs, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedKvPair(status, errorString, kvPairs, handle);
    } else if (m_objectName == businessRuleObject) {
        QnBusinessEventRules rules;

        if(status == 0) {
            try {
                m_serializer.deserializeBusinessRules(rules, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedBusinessRule(status, errorString, rules, handle);
    } else if (m_objectName == settingObject)
    {
        QnKvPairList settings;

        if(status == 0) {
            try {
                m_serializer.deserializeSettings(settings, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedSetting(status, errorString, settings, handle);
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

const QByteArray& QnAppServerConnection::getLastError() const
{
    return m_lastError;
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
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer,connectObject);
    QObject::connect(processor, SIGNAL(finishedConnect(int, const QByteArray&, QnConnectInfoPtr, int)), target, slot);

    QByteArray data;
    return QnSessionManager::instance()->sendAsyncPostRequest(m_url,connectObject, headers, params, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
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

int QnAppServerConnection::connect(QnConnectInfoPtr &connectInfo)
{
    m_lastError.clear();

    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList params;

    QnHTTPRawResponse response;
    int status = QnSessionManager::instance()->sendPostRequest(m_url,connectObject, requestHeaders, params, QByteArray(), response);
    if (status == 0)
    {
        try {
            m_serializer.deserializeConnectInfo(connectInfo, response.data);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
            status = -1;
        }
    } else {
        m_lastError = response.errorString;
    }
    
    return status;
}

QnAppServerConnection::~QnAppServerConnection()
{
}

int QnAppServerConnection::getResourceTypes(QnResourceTypeList& resourceTypes)
{
    m_lastError.clear();

    QnHTTPRawResponse response;

    int status = getObjects(QLatin1String("resourceType"), QString(), response);
    if (status == 0)
    {
        try {
            m_serializer.deserializeResourceTypes(resourceTypes, response.data);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getResources(QnResourceList& resources)
{
    m_lastError.clear();

    QnHTTPRawResponse response;
    int status = getObjects(resourceObject, QString(), response);

    if (status == 0)
    {
        try {
            m_serializer.deserializeResources(resources, response.data, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getResource(const QnId& id, QnResourcePtr& resource)
{
    QnResourceList resources;
    int status = getResources(QString(QLatin1String("id=%1")).arg(id.toString()), resources);
    if (status == 0) 
        resource = resources[0];
    return status;
}

int QnAppServerConnection::getResources(const QString& args, QnResourceList& resources)
{
    m_lastError.clear();

    QnHTTPRawResponse response;

    int status = getObjects(resourceObject, args, response);
    if (status == 0)
    {
        try {
            m_serializer.deserializeResources(resources, response.data, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }
    
    return status;
}

int QnAppServerConnection::registerServer(const QnMediaServerResourcePtr& serverPtr, QnMediaServerResourceList& servers, QByteArray& authKey)
{
    m_lastError.clear();

    QByteArray data;

    m_serializer.serialize(serverPtr, data);

    QnHTTPRawResponse response;
    int status = addObject(serverObject, data, response);
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
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }
    return status;
}

int QnAppServerConnection::addCamera(const QnVirtualCameraResourcePtr& cameraPtr, QnVirtualCameraResourceList& cameras)
{
    m_lastError.clear();

    QByteArray data;

    m_serializer.serialize(cameraPtr, data);

    QnHTTPRawResponse response;
    if (addObject(cameraObject, data, response) == 0)
    {
        try {
            m_serializer.deserializeCameras(cameras, response.data, m_resourceFactory);
            return 0;
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
            cl_log.log(cl_logERROR, "AHTUNG! the client and app server version must be unsynched!");
        }
    } else {
        m_lastError = response.errorString;
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
    else if (QnBusinessEventRulePtr rule = resource.dynamicCast<QnBusinessEventRule>())
        return saveAsync(rule, target, slot);

    return 0;
}

int QnAppServerConnection::addLicenseAsync(const QnLicensePtr &license, QObject *target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, licenseObject);
    QObject::connect(processor, SIGNAL(finishedLicense(int, const QByteArray&, const QnLicenseList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeLicense(license, data);

    return addObjectAsync(licenseObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnKvPairList &kvPairs, QObject *target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, kvPairObject);
    QObject::connect(processor, SIGNAL(finishedKvPair(int, const QByteArray&, const QnKvPairList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeKvPairs(kvPairs, data);

    return addObjectAsync(kvPairObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveSettingsAsync(const QnKvPairList &kvPairs/*, QObject *target, const char *slot*/)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, settingObject);
  //  QObject::connect(processor, SIGNAL(finishedSetting(int,QByteArray,QnKvPairList,int)), target, slot);

    QByteArray data;
    m_serializer.serializeSettings(kvPairs, data);

    return addObjectAsync(settingObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}


int QnAppServerConnection::getResourcesAsync(const QString& args, const QString& objectName, QObject *target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, objectName);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    return getObjectsAsync(objectName, args, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getLicensesAsync(QObject *target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, licenseObject);
    QObject::connect(processor, SIGNAL(finishedLicense(int,QByteArray,QnLicenseList,int)), target, slot);

    return getObjectsAsync(licenseObject, QString(), processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getBusinessRulesAsync(QObject *target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, businessRuleObject);
    QObject::connect(processor, SIGNAL(finishedBusinessRule(int,QByteArray,QnBusinessEventRules,int)), target, slot);

    return getObjectsAsync(businessRuleObject, QString(), processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getSettingsAsync(QObject *target, const char *slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, settingObject);
    QObject::connect(processor, SIGNAL(finishedSetting(int, QByteArray, QnKvPairList, int)), target, slot);

    return getObjectsAsync(settingObject, QString(), processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnUserResourcePtr& userPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, userObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(userPtr, data);

    return addObjectAsync(userObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnMediaServerResourcePtr& serverPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, serverObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(serverPtr, data);

    return addObjectAsync(serverObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnVirtualCameraResourcePtr& cameraPtr, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, cameraObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(cameraPtr, data);

    return addObjectAsync(cameraObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnLayoutResourcePtr& layout, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, layoutObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(layout, data);

    return addObjectAsync(layoutObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnBusinessEventRulePtr& rule, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, businessRuleObject);
    QObject::connect(processor, SIGNAL(finishedBusinessRule(int,QByteArray,QnBusinessEventRules,int)), target, slot);

    QByteArray data;
    m_serializer.serializeBusinessRule(rule, data);

    return addObjectAsync(businessRuleObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnLayoutResourceList& layouts, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, layoutObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeLayouts(layouts, data);

    return addObjectAsync(layoutObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnVirtualCameraResourceList& cameras, QObject* target, const char* slot)
{
    conn_detail::ReplyProcessor* processor = new conn_detail::ReplyProcessor(m_resourceFactory, m_serializer, cameraObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeCameras(cameras, data);

    return addObjectAsync(cameraObject, data, processor, SLOT(finished(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::addCameraHistoryItem(const QnCameraHistoryItem &cameraHistoryItem)
{
    m_lastError.clear();

    QByteArray data;
    m_serializer.serializeCameraServerItem(cameraHistoryItem, data);

    QnHTTPRawResponse response;
    int status = addObject(QLatin1String("cameraServerItem"), data, response);
    if (status)
        m_lastError = response.errorString;

    return status;
}

int QnAppServerConnection::addBusinessRule(const QnBusinessEventRulePtr &businessRule)
{
    m_lastError.clear();

    QByteArray data;
    m_serializer.serializeBusinessRule(businessRule, data);

    QnHTTPRawResponse response;
    const int status = addObject(businessRuleObject, data, response);
    if (status)
        m_lastError = response.errorString;

    return status;
}


int QnAppServerConnection::getServers(QnMediaServerResourceList &servers)
{
    m_lastError.clear();

    QnHTTPRawResponse response;
    int status = getObjects(serverObject, QString(), response);

    if (status == 0) {
        try {
            m_serializer.deserializeServers(servers, response.data, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getCameras(QnVirtualCameraResourceList& cameras, QnId mediaServerId)
{
    m_lastError.clear();

    QnHTTPRawResponse response;

    int status = getObjects(cameraObject, QString(QLatin1String("parent_id=%1")).arg(mediaServerId.toString()), response);
    if (status == 0) 
    {
        try {
            m_serializer.deserializeCameras(cameras, response.data, m_resourceFactory);
        } catch (const QnSerializeException& e) {
            m_lastError += e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getLayouts(QnLayoutResourceList& layouts)
{
    m_lastError.clear();

    QnHTTPRawResponse response;

    int status = getObjects(layoutObject, QString(), response);
    if (status == 0) {
        try {
            m_serializer.deserializeLayouts(layouts, response.data);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getUsers(QnUserResourceList& users)
{
    QnHTTPRawResponse response;

    int status = getObjects(userObject, QString(), response);
    if (status == 0) {
        try {
            m_serializer.deserializeUsers(users, response.data);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getLicenses(QnLicenseList &licenses)
{
    QnHTTPRawResponse response;

    int status = getObjects(licenseObject, QString(), response);
    if (status == 0) {
        try {
            m_serializer.deserializeLicenses(licenses, response.data);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getCameraHistoryList(QnCameraHistoryList &cameraHistoryList)
{
    QnHTTPRawResponse response;

    int status = getObjects(QLatin1String("cameraServerItem"), QString(), response);
    if (status == 0) {
        try {
            m_serializer.deserializeCameraHistoryList(cameraHistoryList, response.data);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
    }

    return status;
}

int QnAppServerConnection::getBusinessRules(QnBusinessEventRules &businessRules)
{
    QnHTTPRawResponse response;
    int status = getObjects(businessRuleObject, QString(), response);

    if (status == 0) {
        try {
            m_serializer.deserializeBusinessRules(businessRules, response.data);
        } catch (const QnSerializeException& e) {
            m_lastError = e.errorString();
        }
    } else {
        m_lastError = response.errorString;
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

    if (appServerConnection->getResourceTypes(resourceTypeList) != 0)
    {
        qWarning() << "Can't get resource types: " << appServerConnection->getLastError();
        return false;
    }

    qnResTypePool->replaceResourceTypeList(resourceTypeList);

    return true;
}

bool initCameraHistory(QnAppServerConnectionPtr appServerConnection)
{
    QnCameraHistoryList cameraHistoryList;
    if (appServerConnection->getCameraHistoryList(cameraHistoryList) != 0)
    {
        qDebug() << "QnMain::run(): Can't get cameras history. Reason: " << appServerConnection->getLastError();
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

    if (appServerConnection->getLicenses(licenses) != 0)
    {
        qDebug() << "Can't get licenses: " << appServerConnection->getLastError();
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

int QnAppServerConnection::saveSync(const QnMediaServerResourcePtr &server)
{
    QByteArray authKey;
    QnMediaServerResourceList servers;
    return registerServer(server, servers, authKey);
}

int QnAppServerConnection::saveSync(const QnVirtualCameraResourcePtr &camera)
{
    QnVirtualCameraResourceList cameras;
    return addCamera(camera, cameras);
}

int QnAppServerConnection::deleteAsync(const QnMediaServerResourcePtr& server, QObject* target, const char* slot)
{
    return deleteObjectAsync(serverObject, server->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnVirtualCameraResourcePtr& camera, QObject* target, const char* slot)
{
    return deleteObjectAsync(cameraObject, camera->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnUserResourcePtr& user, QObject* target, const char* slot)
{
    return deleteObjectAsync(userObject, user->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnLayoutResourcePtr& layout, QObject* target, const char* slot)
{
    return deleteObjectAsync(layoutObject, layout->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnBusinessEventRulePtr& rule, QObject* target, const char* slot)
{
    return deleteObjectAsync(businessRuleObject, rule->getId().toInt(), target, slot);
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
    } else if (QnBusinessEventRulePtr rule = resource.dynamicCast<QnBusinessEventRule>()) {
        return deleteAsync(rule, target, slot);
    } else {
        qWarning() << "Cannot delete resources of type" << resource->metaObject()->className();
        return 0;
    }
}

qint64 QnAppServerConnection::getCurrentTime()
{
    QnHTTPRawResponse response;
    int rez = QnSessionManager::instance()->sendGetRequest(m_url,timeObject, response);
    if (rez != 0) {
        qWarning() << "Can't read time from Enterprise Controller" << response.errorString;
        return QDateTime::currentMSecsSinceEpoch();
    }

    return response.data.toLongLong();
}

int QnAppServerConnection::sendEmail(const QString& addr, const QString& subject, const QString& message)
{
    return sendEmail(QStringList() << addr, subject, message);
}

int QnAppServerConnection::sendEmail(const QStringList& to, const QString& subject, const QString& message)
{
    QnRequestParamList requestParams(m_requestParams);

    QnHTTPRawResponse response;
    QByteArray body;
    m_serializer.serializeEmail(to, subject, message, body);
    int status = QnSessionManager::instance()->sendPostRequest(m_url,emailObject, QnRequestHeaderList(), requestParams, body, response);

    if (status == 0) {
        return 0;
    } else {
        m_lastError = response.errorString;
        qWarning() << "Can't send email " << m_lastError;
        return status;
    }
}

int QnAppServerConnection::setResourceStatusAsync(const QnId &resourceId, QnResource::Status status, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("status", QString::number(status)));

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url,statusObject, requestHeaders, requestParams, "", target, slot);
}

int QnAppServerConnection::setResourceDisabledAsync(const QnId &resourceId, bool disabled, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("disabled", QString::number((int)disabled)));

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url,disabledObject, requestHeaders, requestParams, "", target, slot);
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

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url,statusObject, requestHeaders, requestParams, "", target, slot);
}

int QnAppServerConnection::setResourceStatus(const QnId &resourceId, QnResource::Status status)
{
    m_lastError.clear();

    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("status", QString::number(status)));

    QnHTTPRawResponse response;
    int result = QnSessionManager::instance()->sendPostRequest(m_url, statusObject, requestHeaders, requestParams, "", response);

    if (result)
        m_lastError = response.errorString;

    return result;
}

bool QnAppServerConnection::setPanicMode(QnMediaServerResource::PanicMode value)
{
    m_lastError.clear();

    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("mode", QString::number(value)));

    QnHTTPRawResponse response;
    int result = QnSessionManager::instance()->sendPostRequest(m_url, panicObject, requestHeaders, requestParams, "", response);

    if (result)
        m_lastError = response.errorString;

    return result;
}

bool QnAppServerConnection::dumpDatabase(QByteArray& data)
{
    m_lastError.clear();

    QnHTTPRawResponse response;
    int result = QnSessionManager::instance()->sendGetRequest(m_url, dumpdbObject, m_requestHeaders, m_requestParams, response);

    if (result)
        m_lastError = response.errorString;
    else
        data = response.data;

    return result;
}

bool QnAppServerConnection::restoreDatabase(const QByteArray& data)
{
    m_lastError.clear();

    QnHTTPRawResponse response;
    int result = QnSessionManager::instance()->sendPostRequest(m_url, restoredbObject, m_requestHeaders, m_requestParams, data, response);

    if (result)
        m_lastError = response.errorString;

    return result;
}

bool QnAppServerConnection::broadcastBusinessAction(const QnAbstractBusinessActionPtr& businessAction)
{
    m_lastError.clear();

    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    QByteArray body;
    m_serializer.serializeBusinessAction(businessAction, body);

    QnHTTPRawResponse response;
    int result = QnSessionManager::instance()->sendPostRequest(m_url, bbaObject, requestHeaders, requestParams, body, response);

    if (result)
        m_lastError = response.errorString;

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

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url,disabledObject, requestHeaders, requestParams, "", target, slot);
}

// --------------------------------- QnAppServerConnectionFactory ----------------------------------

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

    return QString();
}

QnResourceFactory* QnAppServerConnectionFactory::defaultFactory()
{
    if (QnAppServerConnectionFactory *factory = theAppServerConnectionFactory()) {
        return factory->m_resourceFactory;
    }

    return 0;
}
