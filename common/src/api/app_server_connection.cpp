#include "app_server_connection.h"

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>

#include <api/serializer/pb_serializer.h>

#include <business/business_event_rule.h>

#include "core/resource/resource_type.h"
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/camera_resource.h"
#include "core/resource/layout_resource.h"
#include "core/resource/user_resource.h"

#include "utils/common/sleep.h"
#include "utils/common/enum_name_mapper.h"
#include "utils/common/synctime.h"
#include "session_manager.h"
#include "message.pb.h"
#include "common_message_processor.h"

namespace {
    QN_DEFINE_NAME_MAPPED_ENUM(RequestObject, 
        ((CameraObject,             "camera"))
        ((CameraServerItemObject,   "cameraServerItem"))
        ((ResourceObject,           "resource"))
        ((ResourceTypeObject,       "resourceType"))
        ((ServerObject,             "server"))
        ((ServerAuthObject,         "server"))
        ((UserObject,               "user"))
        ((LayoutObject,             "layout"))
        ((LicenseObject,            "license"))
        ((BusinessRuleObject,       "businessRule"))
        ((ConnectObject,            "connect"))
        ((DisconnectObject,         "disconnect"))
        ((TimeObject,               "time"))
        ((EmailObject,              "email"))
        ((StatusObject,             "status"))
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
        ((ResetBusinessRulesObject, "resetBusinessRules"))
    );

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnAppServerReplyProcessor
// -------------------------------------------------------------------------- //
QnAppServerReplyProcessor::QnAppServerReplyProcessor(QnResourceFactory &resourceFactory, QnApiSerializer &serializer, int object): 
    QnAbstractReplyProcessor(object),
    m_resourceFactory(resourceFactory),
    m_serializer(serializer)
{
}

QnAppServerReplyProcessor::~QnAppServerReplyProcessor()
{
}

void QnAppServerReplyProcessor::processReply(const QnHTTPRawResponse &response, int handle)
{
    if (response.status != 0)
        m_errorString += QLatin1String(response.errorString) + lit("\n");

    switch(object()) {
    case ServerAuthObject:
    case ServerObject: {
        int status = response.status;

        QnServersReply reply;
        if(status == 0) {
            try {
                m_serializer.deserializeServers(reply.servers, response.data, m_resourceFactory);

                foreach(QNetworkReply::RawHeaderPair rawHeader, response.headers)
                    if (rawHeader.first == "X-NetworkOptix-AuthKey")
                        reply.authKey = rawHeader.second;
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        if(object() == ServerObject) {
            emitFinished(this, status, QnResourceList(reply.servers), handle);
        } else {
            emitFinished(this, status, reply, handle);
        }
        break;
    }
    case CameraObject: {
        int status = response.status;

        QnNetworkResourceList reply;
        if(status == 0) {
            try {
                m_serializer.deserializeCameras(reply, response.data, m_resourceFactory);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, QnResourceList(reply), handle);
        break;
    }
    case CameraServerItemObject: {
        int status = response.status;

        QnCameraHistoryList reply;
        if (status == 0) {
            try {
                m_serializer.deserializeCameraHistoryList(reply, response.data);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, reply, handle);
        break;
    }
    case UserObject: {
        int status = response.status;

        QnUserResourceList reply;
        if(status == 0) {
            try {
                m_serializer.deserializeUsers(reply, response.data);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, QnResourceList(reply), handle);
        break;
    }
    case LayoutObject: {
        int status = response.status;

        QnLayoutResourceList reply;
        if(status == 0) {
            try {
                m_serializer.deserializeLayouts(reply, response.data);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, QnResourceList(reply), handle);
        break;
    }
    case ResourceObject: {
        int status = response.status;

        QnResourceList reply;
        if(status == 0) {
            try {
                m_serializer.deserializeResources(reply, response.data, m_resourceFactory);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, reply, handle);
        break;
    }
    case ResourceTypeObject: {
        int status = response.status;

        QnResourceTypeList reply;
        if (status == 0) {
            try {
                m_serializer.deserializeResourceTypes(reply, response.data);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, reply, handle);
        break;
    }
    case LicenseObject: {
        int status = response.status;

        QnLicenseList reply;
        if(status == 0) {
            try {
                m_serializer.deserializeLicenses(reply, response.data);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, reply, handle);
        break;
    }
    case ConnectObject: {
        int status = response.status;

        QnConnectionInfoPtr reply(new QnConnectionInfo());
        if(status == 0) {
            try {
                m_serializer.deserializeConnectInfo(reply, response.data);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, reply, handle);
        break;
    }
    case KvPairObject: {
        int status = response.status;

        QnKvPairListsById reply;
        if(status == 0) {
            try {
                m_serializer.deserializeKvPairs(reply, response.data);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, reply, handle);
        break;
    }
    case BusinessRuleObject: {
        int status = response.status;

        QnBusinessEventRuleList reply;
        if(status == 0) {
            try {
                m_serializer.deserializeBusinessRules(reply, response.data);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, reply, handle);
        break;
    }
    case SettingObject: {
        int status = response.status;
        
        QnKvPairList reply;
        if(status == 0) {
            try {
                m_serializer.deserializeSettings(reply, response.data);
            } catch (const QnSerializationException& e) {
                m_errorString += e.message();
                status = -1;
            }
        }

        emitFinished(this, status, reply, handle);
        break;
    }
    case TestEmailSettingsObject: {
        if (response.data != "OK")
            m_errorString += QLatin1String(response.data);

        emitFinished(this, response.status, response.data == "OK", handle);
        break;
    }
    case EmailObject: {
        if (response.data != "OK")
            m_errorString += QLatin1String(response.data);

        emitFinished(this, response.status, response.data == "OK", handle);
        break;
    }
    case GetFileObject: {
        emitFinished(this, response.status, response.data, handle);
        break;
    }
    case PutFileObject: {
        emitFinished(this, response.status, handle);
        break;
    }
    case DeleteFileObject: {
        emitFinished(this, response.status, handle);
        break;
    }
    case ListDirObject: {
        int status = response.status;

        QStringList reply;
        if (status == 0) {
            if(!QJson::deserialize(response.data, &reply)) {
                qnWarning("Error parsing JSON reply:\n%1\n\n", response.data);
                status = 1;
            }
        }

        emitFinished(this, status, reply, handle);
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
    m_resourceFactory(resourceFactory),
    m_serializer(serializer)
{
    setUrl(url);
    setNameMapper(new QnEnumNameMapper(QnEnumNameMapper::create<RequestObject>())); // TODO: #Elric no new

    m_requestParams.append(QnRequestParam("format", m_serializer.format()));
    m_requestParams.append(QnRequestParam("guid", guid));
    m_requestHeaders.append(QnRequestHeader(QLatin1String("X-NetworkOptix-AuthKey"), authKey));
}

QnAppServerConnection::~QnAppServerConnection()
{
}

QnAbstractReplyProcessor *QnAppServerConnection::newReplyProcessor(int object) {
    return new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, object);
}

QByteArray QnAppServerConnection::getLastError() const
{
    return "Unknown error"; // TODO: #Elric
}

int QnAppServerConnection::addObject(int object, const QByteArray &data, QVariant *reply)
{
    return sendSyncRequest(QNetworkAccessManager::PostOperation, object, m_requestHeaders, m_requestParams, data, reply);
}

int QnAppServerConnection::addObjectAsync(int object, const QByteArray &data, const char *replyTypeName, QObject *target, const char *slot)
{
    return sendAsyncRequest(QNetworkAccessManager::PostOperation, object, m_requestHeaders, m_requestParams, data, replyTypeName, target, slot);
}

int QnAppServerConnection::deleteObjectAsync(int object, int id, QObject *target, const char *slot)
{
    return QnSessionManager::instance()->sendAsyncDeleteRequest(url(), nameMapper()->name(object), id, target, slot);
}

int QnAppServerConnection::getObjects(int object, const QString &args, QnHTTPRawResponse &response)
{
    QnRequestHeaderList headers(m_requestHeaders);
    QnRequestParamList params(m_requestParams);

    if (!args.isEmpty())
    {
        QStringList argsKvList = args.split(QLatin1Char('='));

        if (argsKvList.length() == 2)
            params.append(QnRequestParam(argsKvList[0], argsKvList[1]));
    }

    return QnSessionManager::instance()->sendSyncGetRequest(url(), nameMapper()->name(object), headers, params, response);
}

int QnAppServerConnection::connectAsync_i(const QnRequestHeaderList &headers, const QnRequestParamList &params, QObject *target, const char *slot)
{
    return sendAsyncGetRequest(ConnectObject, headers, params, QN_STRINGIZE_TYPE(QnConnectionInfoPtr), target, slot);
}

//int QnAppServerConnection::testConnectionAsync(QObject *target, const char *slot)
//{
//    QnRequestParamList params(m_requestParams);
//    params.append(QnRequestParam("ping", "1"));
//
//    return connectAsync_i(m_requestHeaders, params, target, slot);
//}

int QnAppServerConnection::connectAsync(QObject *target, const char *slot)
{
    return connectAsync_i(m_requestHeaders, m_requestParams, target, slot);
}

int QnAppServerConnection::connect(QnConnectionInfoPtr &connectInfo)
{
    return sendSyncGetRequest(ConnectObject, m_requestHeaders, m_requestParams, &connectInfo);
}

int QnAppServerConnection::getResourceTypes(QnResourceTypeList &resourceTypes)
{
    return sendSyncGetRequest(ResourceTypeObject, m_requestHeaders, m_requestParams, &resourceTypes);
}

//int QnAppServerConnection::getResources(QnResourceList &resources)
//{
//    return sendSyncGetRequest(ResourceObject, m_requestHeaders, m_requestParams, &resources);
//}

int QnAppServerConnection::getResource(const QnId &id, QnResourcePtr &resource)
{
    QnRequestParamList params(m_requestParams);
    params.append(QnRequestParam("id", id.toString()));

    QnResourceList reply;
    int status = sendSyncGetRequest(ResourceObject, m_requestHeaders, params, &reply);
    if (status == 0) 
        resource = reply[0];
    return status;
}

int QnAppServerConnection::saveServer(const QnMediaServerResourcePtr &serverPtr, QnMediaServerResourceList &servers, QByteArray &authKey)
{
    QByteArray data;
    m_serializer.serialize(serverPtr, data);

    QnServersReply reply;
    int status = sendSyncRequest(QNetworkAccessManager::PostOperation, ServerAuthObject, m_requestHeaders, m_requestParams, data, &reply);
    if (status == 0) {
        servers = reply.servers;
        authKey = reply.authKey;
    }

    return status;
}

int QnAppServerConnection::addCamera(const QnVirtualCameraResourcePtr &cameraPtr, QnVirtualCameraResourceList &cameras)
{
    QByteArray data;
    m_serializer.serialize(cameraPtr, data);

    QnResourceList reply;
    int status = sendSyncRequest(QNetworkAccessManager::PostOperation, CameraObject, m_requestHeaders, m_requestParams, data, &reply);
    if (status == 0)
        cameras = reply.filtered<QnVirtualCameraResource>();
    return status;
}

//int QnAppServerConnection::saveAsync(const QnResourcePtr &resource, QObject *target, const char *slot)
//{
//    if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
//        return saveAsync(server, target, slot);
//    else if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
//        return saveAsync(camera, target, slot);
//    else if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
//        return saveAsync(user, target, slot);
//    else if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
//        return saveAsync(layout, target, slot);
//    return 0;
//}

//int QnAppServerConnection::addLicensesAsync(const QList<QnLicensePtr> &licenses, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serializeLicenses(licenses, data);
//
//    return addObjectAsync(LicenseObject, data, QN_STRINGIZE_TYPE(QnLicenseList), target, slot);
//}

//int QnAppServerConnection::saveAsync(int resourceId, const QnKvPairList &kvPairs, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serializeKvPairs(resourceId, kvPairs, data);
//
//    return addObjectAsync(KvPairObject, data, QN_STRINGIZE_TYPE(QnKvPairListsById), target, slot);
//}

//int QnAppServerConnection::saveSettingsAsync(const QnKvPairList &kvPairs, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serializeSettings(kvPairs, data);
//
//    return addObjectAsync(SettingObject, data, QN_STRINGIZE_TYPE(QnKvPairList), target, slot);
//}

//int QnAppServerConnection::getLicensesAsync(QObject *target, const char *slot)
//{
//    return sendAsyncGetRequest(LicenseObject, m_requestParams, QN_STRINGIZE_TYPE(QnLicenseList), target, slot);
//}

//int QnAppServerConnection::getBusinessRulesAsync(QObject *target, const char *slot)
//{
//    return sendAsyncGetRequest(BusinessRuleObject, m_requestParams, QN_STRINGIZE_TYPE(QnBusinessEventRuleList), target, slot);
//}

//int QnAppServerConnection::getKvPairsAsync(const QnResourcePtr &resource, QObject *target, const char *slot)
//{
//    QnRequestParamList params(m_requestParams);
//    params.append(QnRequestParam("resource_id", resource->getId().toString()));
//    return sendAsyncGetRequest(KvPairObject, params, QN_STRINGIZE_TYPE(QnKvPairListsById), target, slot);
//}
//
//int QnAppServerConnection::getAllKvPairsAsync(QObject *target, const char *slot)
//{
//    return sendAsyncGetRequest(KvPairObject, m_requestParams, QN_STRINGIZE_TYPE(QnKvPairListsById), target, slot);
//}
//
//int QnAppServerConnection::getSettingsAsync(QObject *target, const char *slot)
//{
//    return sendAsyncGetRequest(SettingObject, m_requestParams, QN_STRINGIZE_TYPE(QnKvPairList), target, slot);
//}

//int QnAppServerConnection::saveAsync(const QnUserResourcePtr &userPtr, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serialize(userPtr, data);
//
//    return addObjectAsync(UserObject, data, QN_STRINGIZE_TYPE(QnResourceList), target, slot);
//}
//
//int QnAppServerConnection::saveAsync(const QnMediaServerResourcePtr &serverPtr, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serialize(serverPtr, data);
//
//    return addObjectAsync(ServerObject, data, QN_STRINGIZE_TYPE(QnResourceList), target, slot);
//}
//
//int QnAppServerConnection::saveAsync(const QnVirtualCameraResourcePtr &cameraPtr, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serialize(cameraPtr, data);
//
//    return addObjectAsync(CameraObject, data, QN_STRINGIZE_TYPE(QnResourceList), target, slot);
//}
//
//int QnAppServerConnection::saveAsync(const QnLayoutResourcePtr &layout, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serialize(layout, data);
//
//    return addObjectAsync(LayoutObject, data, QN_STRINGIZE_TYPE(QnResourceList), target, slot);
//}
//
//int QnAppServerConnection::saveAsync(const QnBusinessEventRulePtr &rule, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serializeBusinessRule(rule, data);
//
//    return addObjectAsync(BusinessRuleObject, data, QN_STRINGIZE_TYPE(QnBusinessEventRuleList), target, slot);
//}
//
//int QnAppServerConnection::saveAsync(const QnLayoutResourceList &layouts, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serializeLayouts(layouts, data);
//
//    return addObjectAsync(LayoutObject, data, QN_STRINGIZE_TYPE(QnResourceList), target, slot);
//}
//
//int QnAppServerConnection::saveAsync(const QnVirtualCameraResourceList &cameras, QObject *target, const char *slot)
//{
//    QByteArray data;
//    m_serializer.serializeCameras(cameras, data);
//
//    return addObjectAsync(CameraObject, data, QN_STRINGIZE_TYPE(QnResourceList), target, slot);
//}

int QnAppServerConnection::addCameraHistoryItem(const QnCameraHistoryItem &cameraHistoryItem)
{
    QByteArray data;
    m_serializer.serializeCameraServerItem(cameraHistoryItem, data);

    return addObject(CameraServerItemObject, data, NULL);
}

int QnAppServerConnection::addBusinessRule(const QnBusinessEventRulePtr &businessRule)
{
    QByteArray data;
    m_serializer.serializeBusinessRule(businessRule, data);

    return addObject(BusinessRuleObject, data, NULL);
}


int QnAppServerConnection::getServers(QnMediaServerResourceList &servers)
{
    QnResourceList reply;
    int status = sendSyncGetRequest(ServerObject, m_requestHeaders, m_requestParams, &reply);
    if(status == 0)
        servers = reply.filtered<QnMediaServerResource>();
    return status;
}

int QnAppServerConnection::getKvPairs(QnKvPairList& kvPairs, const QnResourcePtr &resource)
{
    QnKvPairListsById reply;
    QnRequestParamList params(m_requestParams);
    params.append(QnRequestParam("resource_id", resource->getId().toString()));
    int status = sendSyncGetRequest(KvPairObject, m_requestHeaders, m_requestParams, &reply);
    QnKvPairListsById::const_iterator citer = reply.constFind(resource->getId());
    if (citer != reply.cend())
        kvPairs = citer.value();

    return status;
}

int QnAppServerConnection::getCameras(QnVirtualCameraResourceList &cameras, QnId mediaServerId)
{
    QnRequestParamList params = m_requestParams;
    params.append(QnRequestParam("parent_id", mediaServerId.toString()));

    QnResourceList reply;
    int status = sendSyncGetRequest(CameraObject, m_requestHeaders, params, &reply);
    if(status == 0)
        cameras = reply.filtered<QnVirtualCameraResource>();
    return status;
}

//int QnAppServerConnection::getLayouts(QnLayoutResourceList &layouts)
//{
//    QnResourceList reply;
//    int status = sendSyncGetRequest(LayoutObject, m_requestHeaders, m_requestParams, &reply);
//    if(status == 0)
//        layouts = reply.filtered<QnLayoutResource>();
//    return status;
//}

//int QnAppServerConnection::getUsers(QnUserResourceList &users)
//{
//    QnResourceList reply;
//    int status = sendSyncGetRequest(UserObject, m_requestHeaders, m_requestParams, &reply);
//    if(status == 0)
//        users = reply.filtered<QnUserResource>();
//    return status;
//}

int QnAppServerConnection::getLicenses(QnLicenseList &licenses)
{
    return sendSyncGetRequest(LicenseObject, m_requestHeaders, m_requestParams, &licenses);
}

int QnAppServerConnection::getCameraHistoryList(QnCameraHistoryList &cameraHistoryList)
{
    return sendSyncGetRequest(CameraServerItemObject, m_requestHeaders, m_requestParams, &cameraHistoryList);
}

int QnAppServerConnection::getBusinessRules(QnBusinessEventRuleList &businessRules)
{
    return sendSyncGetRequest(BusinessRuleObject, m_requestHeaders, m_requestParams, &businessRules);
}


int QnAppServerConnection::saveSync(int resourceId, const QnKvPair &kvPair)
{
    return saveSync(resourceId, QnKvPairList() << kvPair);
}

int QnAppServerConnection::saveSync(int resourceId, const QnKvPairList &kvPairs)
{
    QByteArray data;
    m_serializer.serializeKvPairs(resourceId, kvPairs, data);

    return addObject(KvPairObject, data, 0);
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

//int QnAppServerConnection::deleteAsync(const QnMediaServerResourcePtr &server, QObject *target, const char *slot)
//{
//    return deleteObjectAsync(ServerObject, server->getId().toInt(), target, slot);
//}
//
//int QnAppServerConnection::deleteAsync(const QnVirtualCameraResourcePtr &camera, QObject *target, const char *slot)
//{
//    return deleteObjectAsync(CameraObject, camera->getId().toInt(), target, slot);
//}
//
//int QnAppServerConnection::deleteAsync(const QnUserResourcePtr &user, QObject *target, const char *slot)
//{
//    return deleteObjectAsync(UserObject, user->getId().toInt(), target, slot);
//}
//
//int QnAppServerConnection::deleteAsync(const QnLayoutResourcePtr &layout, QObject *target, const char *slot)
//{
//    return deleteObjectAsync(LayoutObject, layout->getId().toInt(), target, slot);
//}

//int QnAppServerConnection::deleteRuleAsync(int ruleId, QObject *target, const char *slot)
//{
//    return deleteObjectAsync(BusinessRuleObject, ruleId, target, slot);
//}

//int QnAppServerConnection::deleteAsync(const QnResourcePtr &resource, QObject *target, const char *slot) {
//    if(QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>()) {
//        return deleteAsync(server, target, slot);
//    } else if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>()) {
//        return deleteAsync(camera, target, slot);
//    } else if(QnUserResourcePtr user = resource.dynamicCast<QnUserResource>()) {
//        return deleteAsync(user, target, slot);
//    } else if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
//        return deleteAsync(layout, target, slot);
//    } else {
//        qWarning() << "Cannot delete resources of type" << resource->metaObject()->className();
//        return 0;
//    }
//}

qint64 QnAppServerConnection::getCurrentTime()
{
    QnHTTPRawResponse response;
    int rez = QnSessionManager::instance()->sendSyncGetRequest(url(), nameMapper()->name(TimeObject), response);
    if (rez != 0) {
        qWarning() << "Can't read time from Enterprise Controller" << response.errorString;
        return QDateTime::currentMSecsSinceEpoch();
    }

    return response.data.toLongLong();
}

int QnAppServerConnection::testEmailSettingsAsync(const QnKvPairList &settings, QObject *target, const char *slot)
{
    QByteArray data;
    m_serializer.serializeSettings(settings, data);

    return addObjectAsync(TestEmailSettingsObject, data, QN_STRINGIZE_TYPE(bool), target, slot);
}

int QnAppServerConnection::sendEmailAsync(const QString &addr, const QString &subject, const QString &message, int timeout, QObject *target, const char *slot)
{
    return sendEmailAsync(QStringList() << addr, subject, message, timeout, target, slot);
}

int QnAppServerConnection::sendEmailAsync(const QStringList &to, const QString &subject, const QString &message, int timeout, QObject *target, const char *slot)
{
    return sendEmailAsync(to, subject, message, QnEmailAttachmentList(), timeout, target, slot);
}

int QnAppServerConnection::sendEmailAsync(const QStringList &to, const QString& subject, const QString& message, const QnEmailAttachmentList& attachments, int timeout, QObject *target, const char *slot) {
    if (message.isEmpty())
        return -1;

    QByteArray data;
    m_serializer.serializeEmail(to, subject, message, attachments, timeout, data);

    return addObjectAsync(EmailObject, data, QN_STRINGIZE_TYPE(bool), target, slot);
}

//int QnAppServerConnection::requestStoredFileAsync(const QString &filename, QObject *target, const char *slot)
//{
//    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, GetFileObject);
//    QObject::connect(processor, SIGNAL(finished(int, const QByteArray &, int)), target, slot);
//
//    return QnSessionManager::instance()->sendAsyncGetRequest(url(),
//                                                             nameMapper()->name(GetFileObject) + QLatin1String("/") + filename,
//                                                             m_requestHeaders,
//                                                             m_requestParams,
//                                                             processor,
//                                                             "processReply");
//}

//int QnAppServerConnection::addStoredFileAsync(const QString &filename, const QByteArray &filedata, QObject *target, const char *slot)
//{
//    QnRequestHeaderList requestHeaders(m_requestHeaders);
//
//    QByteArray bound("------------------------------0d5e4fc23bba");
//
//    QByteArray data("--" + bound);
//    data += "\r\n";
//    data += "Content-Disposition: form-data; name=\"filedata\"; filename=\"" + filename.toLatin1() + "\"\r\n";
//    data += "Content-Type: application/octet-stream\r\n\r\n";
//    data += filedata;
//    data += "\r\n";
//    data += "--" + bound + "--";
//    data += "\r\n";
//
//    requestHeaders.append(QnRequestHeader(QLatin1String("Content-Type"), QLatin1String("multipart/form-data; boundary=" + bound)));
//    requestHeaders.append(QnRequestHeader(QLatin1String("Content-Length"), QString::number(data.length())));
//
//    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, PutFileObject);
//    QObject::connect(processor, SIGNAL(finished(int, int)), target, slot);
//
//    return QnSessionManager::instance()->sendAsyncPostRequest(url(),
//                                                              nameMapper()->name(PutFileObject),
//                                                              requestHeaders,
//                                                              m_requestParams,
//                                                              data,
//                                                              processor,
//                                                              "processReply");
//}

//int QnAppServerConnection::deleteStoredFileAsync(const QString &filename, QObject *target, const char *slot)
//{
//    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, DeleteFileObject);
//    QObject::connect(processor, SIGNAL(finished(int, int)), target, slot);
//
//    return QnSessionManager::instance()->sendAsyncDeleteRequest(url(),
//                                                                nameMapper()->name(DeleteFileObject) + QLatin1String("/") + filename,
//                                                                m_requestHeaders,
//                                                                m_requestParams,
//                                                                processor,
//                                                                "processReply");
//}

//int QnAppServerConnection::requestDirectoryListingAsync(const QString &folderName, QObject *target, const char *slot) {
//    QnAppServerReplyProcessor* processor = new QnAppServerReplyProcessor(m_resourceFactory, m_serializer, ListDirObject);
//    QObject::connect(processor, SIGNAL(finished(int, const QStringList &, int)), target, slot);
//
//    return QnSessionManager::instance()->sendAsyncGetRequest(url(),
//                                                             nameMapper()->name(ListDirObject) + QLatin1String("/") + folderName,
//                                                             m_requestHeaders,
//                                                             m_requestParams,
//                                                             processor,
//                                                             "processReply");
//}

//int QnAppServerConnection::setResourceStatusAsync(const QnId &resourceId, QnResource::Status status, QObject *target, const char *slot)
//{
//    QnRequestHeaderList requestHeaders(m_requestHeaders);
//    QnRequestParamList requestParams(m_requestParams);
//
//    requestParams.append(QnRequestParam("id", resourceId.toString()));
//    requestParams.append(QnRequestParam("status", QString::number(status)));
//
//    return QnSessionManager::instance()->sendAsyncPostRequest(url(), nameMapper()->name(StatusObject), requestHeaders, requestParams, "", target, slot);
//}
//
//int QnAppServerConnection::setResourcesStatusAsync(const QnResourceList &resources, QObject *target, const char *slot)
//{
//    QnRequestHeaderList requestHeaders(m_requestHeaders);
//    QnRequestParamList requestParams(m_requestParams);
//
//    int n = 1;
//    foreach (const QnResourcePtr resource, resources)
//    {
//        requestParams.append(QnRequestParam(QString(QLatin1String("id%1")).arg(n), resource->getId().toString()));
//        requestParams.append(QnRequestParam(QString(QLatin1String("status%1")).arg(n), QString::number(resource->getStatus())));
//
//        n++;
//    }
//
//    return QnSessionManager::instance()->sendAsyncPostRequest(url(), nameMapper()->name(StatusObject), requestHeaders, requestParams, "", target, slot);
//}

int QnAppServerConnection::setResourceStatus(const QnId &resourceId, QnResource::Status status)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("id", resourceId.toString()));
    requestParams.append(QnRequestParam("status", QString::number(status)));

    QnHTTPRawResponse response;
    return QnSessionManager::instance()->sendSyncPostRequest(url(), nameMapper()->name(StatusObject), requestHeaders, requestParams, "", response);
}

bool QnAppServerConnection::setPanicMode(int value)
{
    QnRequestHeaderList requestHeaders(m_requestHeaders);
    QnRequestParamList requestParams(m_requestParams);

    requestParams.append(QnRequestParam("mode", QString::number(value)));

    QnHTTPRawResponse response;
    return QnSessionManager::instance()->sendSyncPostRequest(url(), nameMapper()->name(PanicObject), requestHeaders, requestParams, "", response);
}

void QnAppServerConnection::disconnectSync() {
    QnHTTPRawResponse response;
    QnSessionManager::instance()->sendSyncPostRequest(url(), nameMapper()->name(DisconnectObject), m_requestHeaders, m_requestParams, "", response);
}

//int QnAppServerConnection::dumpDatabaseAsync(QObject *target, const char *slot) {
//    return QnSessionManager::instance()->sendAsyncGetRequest(url(), nameMapper()->name(DumpDbObject), m_requestHeaders, m_requestParams, target, slot);
//}
//
//int QnAppServerConnection::restoreDatabaseAsync(const QByteArray &data, QObject *target, const char *slot) {
//    return QnSessionManager::instance()->sendAsyncPostRequest(url(), nameMapper()->name(RestoreDbObject), m_requestHeaders, m_requestParams, data, target, slot);
//}

//int QnAppServerConnection::broadcastBusinessAction(const QnAbstractBusinessActionPtr &businessAction, QObject *target, const char *slot)
//{
//    QnRequestHeaderList requestHeaders(m_requestHeaders);
//    QnRequestParamList requestParams(m_requestParams);
//
//    QByteArray body;
//    m_serializer.serializeBusinessAction(businessAction, body);
//
//    return QnSessionManager::instance()->sendAsyncPostRequest(url(), nameMapper()->name(BusinessActionObject), requestHeaders, requestParams, body, target, slot);
//}

int QnAppServerConnection::resetBusinessRulesAsync(QObject *target, const char *slot) {
    return QnSessionManager::instance()->sendAsyncPostRequest(url(), nameMapper()->name(ResetBusinessRulesObject), m_requestHeaders, m_requestParams, "", target, slot);
}

// -------------------------------------------------------------------------- //
// QnAppServerConnectionFactory
// -------------------------------------------------------------------------- //
Q_GLOBAL_STATIC(QnAppServerConnectionFactory, qn_appServerConnectionFactory_instance)

QnAppServerConnectionFactory::QnAppServerConnectionFactory(): 
    m_defaultMediaProxyPort(0), 
    m_allowCameraChanges(true),
    m_serializer(new QnApiPbSerializer())
{}

QnAppServerConnectionFactory::~QnAppServerConnectionFactory() {
    return;
}

void QnAppServerConnectionFactory::setDefaultMediaProxyPort(int port)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_defaultMediaProxyPort = port;
    }
}

void QnAppServerConnectionFactory::setCurrentVersion(const QnSoftwareVersion &version)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_currentVersion = version;
    }
}

void QnAppServerConnectionFactory::setSystemName(const QString& systemName)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_systemName = systemName.trimmed();
    }
}

QString QnAppServerConnectionFactory::systemName()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        if (!factory->m_systemName.isEmpty())
            return factory->m_systemName;
    }

    return QString();
}

QByteArray QnAppServerConnectionFactory::prevSessionKey()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
        return factory->m_prevSessionKey;
    return QByteArray();
}


QByteArray QnAppServerConnectionFactory::sessionKey()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
            return factory->m_sessionKey;
    return QByteArray();
}

void QnAppServerConnectionFactory::setSessionKey(const QByteArray& sessionKey)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        if (sessionKey != factory->m_sessionKey) {
            factory->m_prevSessionKey = factory->m_sessionKey;
            factory->m_sessionKey = sessionKey.trimmed();
        }
    }
}

void QnAppServerConnectionFactory::setAllowCameraCHanges(bool value)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance())
            factory->m_allowCameraChanges = value;
}

bool QnAppServerConnectionFactory::isAllowCameraCHanges()
{
    return qn_appServerConnectionFactory_instance()->m_allowCameraChanges;
}

void QnAppServerConnectionFactory::setPublicIp(const QString &publicIp)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_publicUrl.setHost(publicIp);
    }
}

int QnAppServerConnectionFactory::defaultMediaProxyPort()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_defaultMediaProxyPort;
    }

    return 0;
}

QnSoftwareVersion QnAppServerConnectionFactory::currentVersion()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_currentVersion;
    }

    return QnSoftwareVersion();
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

QUrl QnAppServerConnectionFactory::publicUrl()
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        return factory->m_publicUrl;
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
        factory->m_publicUrl = url;
    }
}

void QnAppServerConnectionFactory::setDefaultFactory(QnResourceFactory* resourceFactory)
{
    if (QnAppServerConnectionFactory *factory = qn_appServerConnectionFactory_instance()) {
        factory->m_resourceFactory = resourceFactory;
    }
}

//QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection(const QUrl& url)
//{
//    QUrl urlNoPassword (url);
//    urlNoPassword.setPassword(QString());
//
//    cl_log.log(QLatin1String("Creating connection to the Enterprise Controller ") + urlNoPassword.toString(), cl_logDEBUG2);
//
//    return QnAppServerConnectionPtr(new QnAppServerConnection(
//        url,
//        *(qn_appServerConnectionFactory_instance()->m_resourceFactory),
//        qn_appServerConnectionFactory_instance()->m_serializer,
//        qn_appServerConnectionFactory_instance()->m_clientGuid,
//        qn_appServerConnectionFactory_instance()->m_authKey)
//    );
//}
//
//QnAppServerConnectionPtr QnAppServerConnectionFactory::createConnection()
//{
//    return createConnection(defaultUrl());
//}

static ec2::AbstractECConnectionFactory* ec2ConnectionFactoryInstance = nullptr;

void QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2::AbstractECConnectionFactory* _ec2ConnectionFactory )
{
    ec2ConnectionFactoryInstance = _ec2ConnectionFactory;
}

ec2::AbstractECConnectionFactory* QnAppServerConnectionFactory::ec2ConnectionFactory()
{
    return ec2ConnectionFactoryInstance;
}

static ec2::AbstractECConnectionPtr currentlyUsedEc2Connection;
void QnAppServerConnectionFactory::setEc2Connection( ec2::AbstractECConnectionPtr ec2Connection )
{
    currentlyUsedEc2Connection = ec2Connection;
    QnCommonMessageProcessor::instance()->init(ec2Connection);
}

ec2::AbstractECConnectionPtr QnAppServerConnectionFactory::getConnection2()
{
    return currentlyUsedEc2Connection;
}


#ifdef OLD_EC
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
#endif

bool initResourceTypes(ec2::AbstractECConnectionPtr ec2Connection)
{
    QList<QnResourceTypePtr> resourceTypeList;
    const ec2::ErrorCode errorCode = ec2Connection->getResourceManager()->getResourceTypesSync(&resourceTypeList);
    if( errorCode != ec2::ErrorCode::ok )
    {
        NX_LOG( QString::fromLatin1("Failed to load resource types. %1").arg(ec2::toString(errorCode)), cl_logERROR );
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
