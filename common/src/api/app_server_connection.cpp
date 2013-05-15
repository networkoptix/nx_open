#include "app_server_connection.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include "utils/common/sleep.h"
#include "utils/common/enum_name_mapper.h"
#include "utils/common/synctime.h"
#include "session_manager.h"
#include "message.pb.h"

#include <business/actions/abstract_business_action.h>

namespace {
    QN_DEFINE_NAME_MAPPED_ENUM(RequestObject, 
        ((CameraObject,             "camera"))
        ((CameraServerItemObject,   "cameraServerItem"))
        ((ResourceObject,           "resource"))
        ((ResourceTypeObject,       "resourceType"))
        ((ServerObject,             "server"))
        ((UserObject,               "user"))
        ((LayoutObject,             "layout"))
        ((LicenseObject,            "license"))
        ((BusinessRuleObject,       "businessRule"))
        ((ConnectObject,            "connect"))
        ((TimeObject,               "time"))
        ((EmailObject,              "email"))
        ((StatusObject,             "status"))
        ((DisabledObject,           "disabled"))
        ((PanicObject,              "panic"))
        ((BusinessActionObject,     "businessAction"))
        ((KvPairObject,             "kvPair"))
        ((DumpDbObject,             "dumpdb"))
        ((RestoreDbObject,          "restoredb"))
        ((SettingObject,            "setting"))
        ((TestEmailSettingsObject,  "testEmailSettings"))
        ((GetFileObject,            "getfile"))
        ((PutFileObject,            "putfile"))
        ((DeleteFileObject,         "rmfile"))
        ((ListDirObject,            "listdir"))
    );

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnAppServerReplyProcessor
// -------------------------------------------------------------------------- //
QnAppServerReplyProcessor::QnAppServerReplyProcessor(QnResourceFactory &resourceFactory, QnApiSerializer &serializer, int object): 
    m_resourceFactory(resourceFactory),
    m_serializer(serializer),
    m_object(object)
{
}

QnAppServerReplyProcessor::~QnAppServerReplyProcessor()
{
}

void QnAppServerReplyProcessor::processReply(const QnHTTPRawResponse &response, int handle)
{
    const QByteArray &result = response.data;
    int status = response.status;
    QByteArray errorString = response.errorString;

    if (status != 0)
        errorString += "\n" + response.data;

    switch(m_object) {
    case ServerObject: {
        QnMediaServerResourceList servers;

        if(status == 0) {
            try {
                m_serializer.deserializeServers(servers, result, m_resourceFactory);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, QnResourceList(servers), handle);
        break;
    }
    case CameraObject: {
        QnVirtualCameraResourceList cameras;

        if(status == 0) {
            try {
                m_serializer.deserializeCameras(cameras, result, m_resourceFactory);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, QnResourceList(cameras), handle);
        break;
    }
    case UserObject: {
        QnUserResourceList users;

        if(status == 0) {
            try {
                m_serializer.deserializeUsers(users, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, QnResourceList(users), handle);
        break;
    }
    case LayoutObject: {
        QnLayoutResourceList layouts;

        if(status == 0) {
            try {
                m_serializer.deserializeLayouts(layouts, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, QnResourceList(layouts), handle);
        break;
    }
    case ResourceObject: {
        QnResourceList resources;

        if(status == 0) {
            try {
                m_serializer.deserializeResources(resources, result, m_resourceFactory);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finished(status, errorString, resources, handle);
        break;
    }
    case LicenseObject: {
        QnLicenseList licenses;

        if(status == 0) {
            try {
                m_serializer.deserializeLicenses(licenses, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedLicense(status, errorString, licenses, handle);
        break;
    }
    case ConnectObject: {
        QnConnectInfoPtr connectInfo(new QnConnectInfo());

        if(status == 0) {
            try {
                m_serializer.deserializeConnectInfo(connectInfo, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedConnect(status, errorString, connectInfo, handle);
        break;
    }
    case KvPairObject: {


        QnKvPairs kvPairs;

        if(status == 0) {
            try {
                m_serializer.deserializeKvPairs(kvPairs, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedKvPair(status, errorString, kvPairs, handle);
        break;
    }
    case BusinessRuleObject: {
        QnBusinessEventRules rules;

        if(status == 0) {
            try {
                m_serializer.deserializeBusinessRules(rules, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedBusinessRule(status, errorString, rules, handle);
        break;
    }
    case SettingObject: {
        QnKvPairList settings;

        if(status == 0) {
            try {
                m_serializer.deserializeSettings(settings, result);
            } catch (const QnSerializeException& e) {
                errorString += e.errorString();
            }
        }

        emit finishedSetting(status, errorString, settings, handle);
        break;
    }
    case TestEmailSettingsObject: {
        if (result != "OK")
            errorString = result;

        emit finishedTestEmailSettings(status, errorString, result == "OK", handle);
        break;
    }
    case EmailObject: {
        if (result != "OK")
            errorString += result;

        emit finishedSendEmail(status, errorString, result == "OK", handle);
        break;
    }
    case GetFileObject: {
            emit finishedGetFile(status, result, handle);
            break;
        }
    case PutFileObject: {
            emit finishedPutFile(status, handle);
            break;
        }
    case DeleteFileObject: {
            emit finishedDeleteFile(status, handle);
            break;
        }
    case ListDirObject: {
            QStringList filenames;
            if (status == 0) {
                if(!QJson::deserialize(result, &filenames)) {
                    qnWarning("Error parsing JSON reply:\n%1\n\n", result);
                    status = 1;
                }
            }
            emit finishedDirectoryListing(status, filenames, handle);
            break;
        }
    default:
        assert(false); /* We should never get here. */
        break;
    }

    deleteLater();
}


// -------------------------------------------------------------------------- //
// QnAppServerConnection
// -------------------------------------------------------------------------- //
QnAppServerConnection::QnAppServerConnection(const QUrl &url, QnResourceFactory &resourceFactory, QnApiSerializer &serializer, const QString &guid, const QString &authKey): 
    m_url(url),
    m_resourceFactory(resourceFactory),
    m_serializer(serializer),
    m_objectNameMapper(new QnEnumNameMapper(createEnumNameMapper<RequestObject>()))
{
    m_requestParams.append(QnRequestParam("format", m_serializer.format()));
    m_requestParams.append(QnRequestParam("guid", guid));
    m_requestHeaders.append(QnRequestHeader(QLatin1String("X-NetworkOptix-AuthKey"), authKey));
}

const QByteArray& QnAppServerConnection::getLastError() const
{
    return m_lastError;
}

int QnAppServerConnection::addObject(int object, const QByteArray &data, QnHTTPRawResponse &response)
{
    int status = QnSessionManager::instance()->sendPostRequest(m_url, m_objectNameMapper->name(object), m_requestHeaders, m_requestParams, data, response);
    if (status != 0)
    {
        response.errorString += "\nSessionManager::sendPostRequest(): ";
        response.errorString += QnSessionManager::formatNetworkError(status) + response.data;
    }

    return status;
}

int QnAppServerConnection::getObjectsAsync(int object, const QString &args, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    if (!args.isEmpty())
    {
        QStringList argsKvList = args.split(QLatin1Char('='));

        if (argsKvList.length() == 2)
            requestParams.append(QnRequestParam(argsKvList[0], argsKvList[1]));
    }

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, m_objectNameMapper->name(object), requestHeaders, requestParams, target, slot);
}

int QnAppServerConnection::addObjectAsync(int object, const QByteArray &data, QObject *target, const char *slot)
{
    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, m_objectNameMapper->name(object), m_requestHeaders, m_requestParams, data, target, slot);
}

int QnAppServerConnection::deleteObjectAsync(int object, int id, QObject *target, const char *slot)
{
    return QnSessionManager::instance()->sendAsyncDeleteRequest(m_url, m_objectNameMapper->name(object), id, target, slot);
}

int QnAppServerConnection::getObjects(int object, const QString &args, QnHTTPRawResponse &response)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    if (!args.isEmpty())
    {
        QStringList argsKvList = args.split(QLatin1Char('='));

        if (argsKvList.length() == 2)
            requestParams.append(QnRequestParam(argsKvList[0], argsKvList[1]));
    }

    return QnSessionManager::instance()->sendGetRequest(m_url, m_objectNameMapper->name(object), requestHeaders, requestParams, response);
}

int QnAppServerConnection::connectAsync_i(const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, ConnectObject);
    QObject::connect(processor, SIGNAL(finishedConnect(int, const QByteArray&, QnConnectInfoPtr, int)), target, slot);

    QByteArray data;
    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, m_objectNameMapper->name(ConnectObject), headers, params, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::testConnectionAsync(QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("ping", "1"));

    return connectAsync_i(requestHeaders, requestParams, target, slot);
}

int QnAppServerConnection::connectAsync(QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    return connectAsync_i(requestHeaders, requestParams, target, slot);
}

int QnAppServerConnection::connect(QnConnectInfoPtr &connectInfo)
{
    m_lastError.clear();

    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    QnHTTPRawResponse response;
    int status = QnSessionManager::instance()->sendPostRequest(m_url, m_objectNameMapper->name(ConnectObject), requestHeaders, requestParams, QByteArray(), response);
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

int QnAppServerConnection::getResourceTypes(QnResourceTypeList &resourceTypes)
{
    m_lastError.clear();

    QnHTTPRawResponse response;

    int status = getObjects(ResourceTypeObject, QString(), response);
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

int QnAppServerConnection::getResources(QnResourceList &resources)
{
    m_lastError.clear();

    QnHTTPRawResponse response;
    int status = getObjects(ResourceObject, QString(), response);

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

int QnAppServerConnection::getResource(const QnId &id, QnResourcePtr &resource)
{
    QnResourceList resources;
    int status = getResources(QString(QLatin1String("id=%1")).arg(id.toString()), resources);
    if (status == 0) 
        resource = resources[0];
    return status;
}

int QnAppServerConnection::getResources(const QString &args, QnResourceList &resources)
{
    m_lastError.clear();

    QnHTTPRawResponse response;

    int status = getObjects(ResourceObject, args, response);
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

int QnAppServerConnection::saveServer(const QnMediaServerResourcePtr &serverPtr, QnMediaServerResourceList &servers, QByteArray &authKey)
{
    m_lastError.clear();

    QByteArray data;

    m_serializer.serialize(serverPtr, data);

    QnHTTPRawResponse response;
    int status = addObject(ServerObject, data, response);
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

int QnAppServerConnection::addCamera(const QnVirtualCameraResourcePtr &cameraPtr, QnVirtualCameraResourceList &cameras)
{
    m_lastError.clear();

    QByteArray data;

    m_serializer.serialize(cameraPtr, data);

    QnHTTPRawResponse response;
    if (addObject(CameraObject, data, response) == 0)
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

int QnAppServerConnection::saveAsync(const QnResourcePtr &resource, QObject *target, const char *slot)
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

int QnAppServerConnection::addLicensesAsync(const QList<QnLicensePtr> &licenses, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, LicenseObject);
    QObject::connect(processor, SIGNAL(finishedLicense(int, const QByteArray&, const QnLicenseList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeLicenses(licenses, data);

    return addObjectAsync(LicenseObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnResourcePtr &resource, const QnKvPairList &kvPairs, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, KvPairObject);
    QObject::connect(processor, SIGNAL(finishedKvPair(int, const QByteArray&, const QnKvPairs&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeKvPairs(resource, kvPairs, data);

    return addObjectAsync(KvPairObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveSettingsAsync(const QnKvPairList &kvPairs/*, QObject *target, const char *slot*/)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, SettingObject);
  //  QObject::connect(processor, SIGNAL(finishedSetting(int,QByteArray,QnKvPairList,int)), target, slot);

    QByteArray data;
    m_serializer.serializeSettings(kvPairs, data);

    return addObjectAsync(SettingObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getResourcesAsync(const QString &args, int object, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, object);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    return getObjectsAsync(object, args, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getLicensesAsync(QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, LicenseObject);
    QObject::connect(processor, SIGNAL(finishedLicense(int,QByteArray,QnLicenseList,int)), target, slot);

    return getObjectsAsync(LicenseObject, QString(), processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getBusinessRulesAsync(QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, BusinessRuleObject);
    QObject::connect(processor, SIGNAL(finishedBusinessRule(int,QByteArray,QnBusinessEventRules,int)), target, slot);

    return getObjectsAsync(BusinessRuleObject, QString(), processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getKvPairsAsync(QObject *target, const char *slot) 
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, KvPairObject);
    QObject::connect(processor, SIGNAL(finishedKvPair(int, QByteArray, QnKvPairs, int)), target, slot);

    return getObjectsAsync(KvPairObject, QString(), processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::getSettingsAsync(QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, SettingObject);
    QObject::connect(processor, SIGNAL(finishedSetting(int, QByteArray, QnKvPairList, int)), target, slot);

    return getObjectsAsync(SettingObject, QString(), processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnUserResourcePtr &userPtr, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, UserObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(userPtr, data);

    return addObjectAsync(UserObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnMediaServerResourcePtr &serverPtr, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, ServerObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(serverPtr, data);

    return addObjectAsync(ServerObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnVirtualCameraResourcePtr &cameraPtr, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, CameraObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(cameraPtr, data);

    return addObjectAsync(CameraObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnLayoutResourcePtr &layout, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, LayoutObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serialize(layout, data);

    return addObjectAsync(LayoutObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnBusinessEventRulePtr &rule, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, BusinessRuleObject);
    QObject::connect(processor, SIGNAL(finishedBusinessRule(int,QByteArray,QnBusinessEventRules,int)), target, slot);

    QByteArray data;
    m_serializer.serializeBusinessRule(rule, data);

    return addObjectAsync(BusinessRuleObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnLayoutResourceList &layouts, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, LayoutObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeLayouts(layouts, data);

    return addObjectAsync(LayoutObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::saveAsync(const QnVirtualCameraResourceList &cameras, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, CameraObject);
    QObject::connect(processor, SIGNAL(finished(int, const QByteArray&, const QnResourceList&, int)), target, slot);

    QByteArray data;
    m_serializer.serializeCameras(cameras, data);

    return addObjectAsync(CameraObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::addCameraHistoryItem(const QnCameraHistoryItem &cameraHistoryItem)
{
    m_lastError.clear();

    QByteArray data;
    m_serializer.serializeCameraServerItem(cameraHistoryItem, data);

    QnHTTPRawResponse response;
    int status = addObject(CameraServerItemObject, data, response);
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
    const int status = addObject(BusinessRuleObject, data, response);
    if (status)
        m_lastError = response.errorString;

    return status;
}


int QnAppServerConnection::getServers(QnMediaServerResourceList &servers)
{
    m_lastError.clear();

    QnHTTPRawResponse response;
    int status = getObjects(ServerObject, QString(), response);

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

int QnAppServerConnection::getCameras(QnVirtualCameraResourceList &cameras, QnId mediaServerId)
{
    m_lastError.clear();

    QnHTTPRawResponse response;

    int status = getObjects(CameraObject, QString(QLatin1String("parent_id=%1")).arg(mediaServerId.toString()), response);
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

int QnAppServerConnection::getLayouts(QnLayoutResourceList &layouts)
{
    m_lastError.clear();

    QnHTTPRawResponse response;

    int status = getObjects(LayoutObject, QString(), response);
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

int QnAppServerConnection::getUsers(QnUserResourceList &users)
{
    QnHTTPRawResponse response;

    int status = getObjects(UserObject, QString(), response);
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

    int status = getObjects(LicenseObject, QString(), response);
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

    int status = getObjects(CameraServerItemObject, QString(), response);
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
    int status = getObjects(BusinessRuleObject, QString(), response);

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

int QnAppServerConnection::saveSync(const QnMediaServerResourcePtr &server)
{
    QByteArray authKey;
    QnMediaServerResourceList servers;
    return saveServer(server, servers, authKey);
}

int QnAppServerConnection::saveSync(const QnVirtualCameraResourcePtr &camera)
{
    QnVirtualCameraResourceList cameras;
    return addCamera(camera, cameras);
}

int QnAppServerConnection::deleteAsync(const QnMediaServerResourcePtr &server, QObject *target, const char *slot)
{
    return deleteObjectAsync(ServerObject, server->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnVirtualCameraResourcePtr &camera, QObject *target, const char *slot)
{
    return deleteObjectAsync(CameraObject, camera->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnUserResourcePtr &user, QObject *target, const char *slot)
{
    return deleteObjectAsync(UserObject, user->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteAsync(const QnLayoutResourcePtr &layout, QObject *target, const char *slot)
{
    return deleteObjectAsync(LayoutObject, layout->getId().toInt(), target, slot);
}

int QnAppServerConnection::deleteRuleAsync(int ruleId, QObject *target, const char *slot)
{
    return deleteObjectAsync(BusinessRuleObject, ruleId, target, slot);
}

int QnAppServerConnection::deleteAsync(const QnResourcePtr &resource, QObject *target, const char *slot) {
    if(QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>()) {
        return deleteAsync(server, target, slot);
    } else if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>()) {
        return deleteAsync(camera, target, slot);
    } else if(QnUserResourcePtr user = resource.dynamicCast<QnUserResource>()) {
        return deleteAsync(user, target, slot);
    } else if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
        return deleteAsync(layout, target, slot);
    } else {
        qWarning() << "Cannot delete resources of type" << resource->metaObject()->className();
        return 0;
    }
}

qint64 QnAppServerConnection::getCurrentTime()
{
    QnHTTPRawResponse response;
    int rez = QnSessionManager::instance()->sendGetRequest(m_url, m_objectNameMapper->name(TimeObject), response);
    if (rez != 0) {
        qWarning() << "Can't read time from Enterprise Controller" << response.errorString;
        return QDateTime::currentMSecsSinceEpoch();
    }

    return response.data.toLongLong();
}

int QnAppServerConnection::testEmailSettingsAsync(const QnKvPairList &settings, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, TestEmailSettingsObject);
    QObject::connect(processor, SIGNAL(finishedTestEmailSettings(int, const QByteArray&, bool, int)), target, slot);

    QByteArray data;
    m_serializer.serializeSettings(settings, data);

    return addObjectAsync(TestEmailSettingsObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::sendEmailAsync(const QString &addr, const QString &subject, const QString &message, int timeout, QObject *target, const char *slot)
{
    return sendEmailAsync(QStringList() << addr, subject, message, timeout, target, slot);
}

int QnAppServerConnection::sendEmailAsync(const QStringList &to, const QString &subject, const QString &message, int timeout, QObject *target, const char *slot)
{
    if (message.isEmpty())
        return -1;

    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, EmailObject);
    QObject::connect(processor, SIGNAL(finishedSendEmail(int, const QByteArray&, bool, int)), target, slot);

    QByteArray data;
    m_serializer.serializeEmail(to, subject, message, timeout, data);

    return addObjectAsync(EmailObject, data, processor, SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::requestStoredFileAsync(const QString &filename, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, GetFileObject);
    QObject::connect(processor, SIGNAL(finishedGetFile(int, const QByteArray&, int)), target, slot);

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url,
                                                             m_objectNameMapper->name(GetFileObject) + QLatin1String("/") + filename,
                                                             m_requestHeaders,
                                                             m_requestParams,
                                                             processor,
                                                             SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::addStoredFileAsync(const QString &filename, const QByteArray &filedata, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);

    QByteArray bound("------------------------------0d5e4fc23bba");

    QByteArray data("--" + bound);
    data += "\r\n";
    data += "Content-Disposition: form-data; name=\"filedata\"; filename=\"" + filename.toAscii() + "\"\r\n";
    data += "Content-Type: application/octet-stream\r\n\r\n";
    data += filedata;
    data += "\r\n";
    data += "--" + bound + "--";
    data += "\r\n";

    requestHeaders.append(QnRequestHeader(QLatin1String("Content-Type"), QLatin1String("multipart/form-data; boundary=" + bound)));
    requestHeaders.append(QnRequestHeader(QLatin1String("Content-Length"), QString::number(data.length())));

    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, PutFileObject);
    QObject::connect(processor, SIGNAL(finishedPutFile(int, int)), target, slot);

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url,
                                                              m_objectNameMapper->name(PutFileObject),
                                                              requestHeaders,
                                                              m_requestParams,
                                                              data,
                                                              processor,
                                                              SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::deleteStoredFileAsync(const QString &filename, QObject *target, const char *slot)
{
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, DeleteFileObject);
    QObject::connect(processor, SIGNAL(finishedDeleteFile(int, int)), target, slot);

    return QnSessionManager::instance()->sendAsyncDeleteRequest(m_url,
                                                                m_objectNameMapper->name(DeleteFileObject) + QLatin1String("/") + filename,
                                                                m_requestHeaders,
                                                                m_requestParams,
                                                                processor,
                                                                SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::requestDirectoryListingAsync(const QString &folderName, QObject *target, const char *slot) {
    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, ListDirObject);
    QObject::connect(processor, SIGNAL(finishedDirectoryListing(int, const QStringList&, int)), target, slot);

    return QnSessionManager::instance()->sendAsyncGetRequest(m_url,
                                                             m_objectNameMapper->name(ListDirObject) + QLatin1String("/") + folderName,
                                                             m_requestHeaders,
                                                             m_requestParams,
                                                             processor,
                                                             SLOT(processReply(QnHTTPRawResponse, int)));
}

int QnAppServerConnection::setResourceStatusAsync(const QnId &resourceId, QnResource::Status status, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("status", QString::number(status)));

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, m_objectNameMapper->name(StatusObject), requestHeaders, requestParams, "", target, slot);
}

int QnAppServerConnection::setResourceDisabledAsync(const QnId &resourceId, bool disabled, QObject *target, const char *slot)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("disabled", QString::number((int)disabled)));

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, m_objectNameMapper->name(DisabledObject), requestHeaders, requestParams, "", target, slot);
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

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, m_objectNameMapper->name(StatusObject), requestHeaders, requestParams, "", target, slot);
}

int QnAppServerConnection::setResourceStatus(const QnId &resourceId, QnResource::Status status)
{
    m_lastError.clear();

    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("status", QString::number(status)));

    QnHTTPRawResponse response;
    int result = QnSessionManager::instance()->sendPostRequest(m_url, m_objectNameMapper->name(StatusObject), requestHeaders, requestParams, "", response);

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
    int result = QnSessionManager::instance()->sendPostRequest(m_url, m_objectNameMapper->name(PanicObject), requestHeaders, requestParams, "", response);

    if (result)
        m_lastError = response.errorString;

    return result;
}

int QnAppServerConnection::dumpDatabase(QObject *target, const char *slot) {
    return QnSessionManager::instance()->sendAsyncGetRequest(m_url, m_objectNameMapper->name(DumpDbObject), m_requestHeaders, m_requestParams, target, slot);
}

int QnAppServerConnection::restoreDatabase(const QByteArray &data, QObject *target, const char *slot) {
    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, m_objectNameMapper->name(RestoreDbObject), m_requestHeaders, m_requestParams, data, target, slot);
}

int QnAppServerConnection::broadcastBusinessAction(const QnAbstractBusinessActionPtr &businessAction, QObject *target, const char *slot)
{
    m_lastError.clear();

    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    QByteArray body;
    m_serializer.serializeBusinessAction(businessAction, body);

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, m_objectNameMapper->name(BusinessActionObject), requestHeaders, requestParams, body, target, slot);
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

    return QnSessionManager::instance()->sendAsyncPostRequest(m_url, m_objectNameMapper->name(DisabledObject), requestHeaders, requestParams, "", target, slot);
}


// -------------------------------------------------------------------------- //
// QnAppServerConnectionFactory
// -------------------------------------------------------------------------- //
Q_GLOBAL_STATIC(QnAppServerConnectionFactory, qn_appServerConnectionFactory_instance)

void QnAppServerConnectionFactory::setDefaultMediaProxyPort(int port)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_defaultMediaProxyPort = port;
    }
}

void QnAppServerConnectionFactory::setCurrentVersion(const QString &version)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_currentVersion = version;
    }
}

int QnAppServerConnectionFactory::defaultMediaProxyPort()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_defaultMediaProxyPort;
    }

    return 0;
}

QString QnAppServerConnectionFactory::currentVersion()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_currentVersion;
    }

    return QString();
}

QnResourceFactory* QnAppServerConnectionFactory::defaultFactory()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_resourceFactory;
    }

    return 0;
}

QString QnAppServerConnectionFactory::authKey()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_authKey;
    }

    return QString();
}

QString QnAppServerConnectionFactory::clientGuid()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_clientGuid;
    }

    return QString();
}

QUrl QnAppServerConnectionFactory::defaultUrl()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_defaultUrl;
    }

    return QUrl();
}

void QnAppServerConnectionFactory::setAuthKey(const QString &authKey)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_authKey = authKey;
    }
}

void QnAppServerConnectionFactory::setClientGuid(const QString &guid)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_clientGuid = guid;
    }
}

void QnAppServerConnectionFactory::setDefaultUrl(const QUrl &url)
{
    Q_ASSERT_X(url.isValid(), "QnAppServerConnectionFactory::initialize()", "an invalid url was passed");
    Q_ASSERT_X(!url.isRelative(), "QnAppServerConnectionFactory::initialize()", "relative urls aren't supported");

    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_defaultUrl = url;
    }
}

void QnAppServerConnectionFactory::setDefaultFactory(QnResourceFactory* resourceFactory)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_resourceFactory = resourceFactory;
    }
}

QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection(const QUrl& url)
{
    QUrl urlNoPassword (url);
    urlNoPassword.setPassword(QString());

    cl_log.log(QLatin1String("Creating connection to the Enterprise Controller ") + urlNoPassword.toString(), cl_logDEBUG2);

    return QnAppServerConnectionPtr(new QnAppServerConnection(
        url,
        *(qn_appServerConnectionFactory_instance()->m_resourceFactory),
        qn_appServerConnectionFactory_instance()->m_serializer,
        qn_appServerConnectionFactory_instance()->m_clientGuid,
        qn_appServerConnectionFactory_instance()->m_authKey)
    );
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

    qnLicensePool->replaceLicenses(licenses);

    return true;
}
