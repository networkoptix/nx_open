#include "manual_camera_addition_handler.h"

#include <QtNetwork/QAuthenticator>
#include <QtConcurrent/QtConcurrentRun>

#include <api/media_server_cameras_data.h>
#include <core/resource_managment/resource_discovery_manager.h>
#include <core/resource/resource.h>
#include <core/resource_managment/resource_searcher.h>
#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <utils/common/json.h>
#include <utils/network/tcp_connection_priv.h>


QnManualCameraAdditionHandler::QnManualCameraAdditionHandler()
{

}

QHostAddress QnManualCameraAdditionHandler::parseAddrParam(const QString &param, QString &errStr)
{
    int ip4Addr = inet_addr(param.toLatin1().data());
    if (ip4Addr != 0 && ip4Addr != -1)
        return QHostAddress(param);
    else {
        QHostInfo hi = QHostInfo::fromName(param);
        if (!hi.addresses().isEmpty())
            return hi.addresses()[0];
        else
            errStr = tr("Can't resolve domain name %1").arg(param);
    }
    return QHostAddress();
}

void searchResourcesAsync(const QUuid &processUuid, const QString &startAddr, const QString &endAddr, const QAuthenticator& auth, int port) {
    QnResourceDiscoveryManager::instance()->searchResources(processUuid, startAddr, endAddr, auth, port);
}

int QnManualCameraAdditionHandler::searchStartAction(const QnRequestParamList &params, QByteArray &resultByteArray, QByteArray &contentType)
{
    contentType = "application/json";

    int port = 0;
    QAuthenticator auth;
    auth.setUser("admin");
    auth.setPassword("admin"); // default values

    QString addr1;
    QString addr2;

    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "start_ip") 
            addr1 = param.second;
        else if (param.first == "end_ip")
            addr2 = param.second;
        else if (param.first == "user")
            auth.setUser(param.second);
        else if (param.first == "password")
            auth.setPassword(param.second);
        else if (param.first == "port")
            port = param.second.toInt();
    }

    if (addr1.isNull())
        return CODE_INVALID_PARAMETER;

    if (addr2 == addr1)
        addr2.clear();

    QUuid processUuid = QUuid::createUuid();

    QtConcurrent::run(&searchResourcesAsync, processUuid, addr1, addr2, auth, port);

    QnManualCameraSearchProcessReply reply;
    reply.status = QnManualCameraSearchStatus(QnManualCameraSearchStatus::Init, 0, 1);
    reply.processUuid = processUuid;

    QJson::serialize(reply, &resultByteArray);
    return CODE_OK;
}

int QnManualCameraAdditionHandler::searchStatusAction(const QnRequestParamList &params, QByteArray &resultByteArray, QByteArray &contentType) {
    contentType = "application/json";

    QUuid processUuid;
    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "uuid")
            processUuid = QUuid(param.second);
    }

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    QnManualCameraSearchStatus status;
    if (!QnResourceDiscoveryManager::instance()->getSearchStatus(processUuid, status))
        return CODE_NOT_FOUND;

    QnManualCameraSearchProcessReply reply;
    reply.status = status;
    reply.processUuid = processUuid;
    reply.cameras = QnResourceDiscoveryManager::instance()->getSearchResults(processUuid);

    QJson::serialize(reply, &resultByteArray);
    return CODE_OK;
}


int QnManualCameraAdditionHandler::searchStopAction(const QnRequestParamList &params, QByteArray &resultByteArray, QByteArray &contentType) {
    contentType = "application/json";

    QUuid processUuid;
    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "uuid")
            processUuid = QUuid(param.second);
    }

    qDebug() << "search stop action received" << processUuid;

    if (processUuid.isNull())
        return CODE_INVALID_PARAMETER;

    QnManualCameraSearchProcessReply reply;

    QnManualCameraSearchStatus status(QnManualCameraSearchStatus::Aborted, 1, 1);
    reply.status = status;
    reply.processUuid = processUuid;
    reply.cameras = QnResourceDiscoveryManager::instance()->getSearchResults(processUuid);

    QnResourceDiscoveryManager::instance()->clearSearch(processUuid);

    QJson::serialize(reply, &resultByteArray);
    return CODE_OK;
}


int QnManualCameraAdditionHandler::addAction(const QnRequestParamList &params, QByteArray &resultByteArray, QByteArray &contentType)
{
    Q_UNUSED(contentType)

    QAuthenticator auth;
    QString resType;
    QUrl url;

    QnManualCamerasMap cameras;
    QString errStr;

    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "user")
            auth.setUser(param.second);
        else if (param.first == "password")
            auth.setPassword(param.second);
    }

    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "url") 
        {
            if (!url.isEmpty()) {
                QnManualCamerasMap::iterator itr = cameras.insert(url.toString(), QnManualCameraInfo(url, auth, resType));
                if (itr.value().resType == 0) {
                    errStr = QString("Unknown resource type %1").arg(resType);
                    break;
                }
            }
            url = param.second;
        }
        else if (param.first == "manufacturer")
            resType = param.second;
    }
    if (!url.isEmpty()) {
        QnManualCamerasMap::iterator itr = cameras.insert(url.toString(), QnManualCameraInfo(url, auth, resType));
        if (itr.value().resType == 0) {
            errStr = QString("Unknown resource type %1").arg(resType);
        }
    }
    
    if (!errStr.isEmpty())
    {
        resultByteArray.append("<?xml version=\"1.0\"?>\n");
        resultByteArray.append("<root>\n");
        resultByteArray.append(errStr).append("\n");
        resultByteArray.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }


    resultByteArray.append("<?xml version=\"1.0\"?>\n");
    resultByteArray.append("<root>\n");
    bool registered = QnResourceDiscoveryManager::instance()->registerManualCameras(cameras);
    if (registered) {
        //QnResourceDiscoveryManager::instance().processManualAddedResources();
        resultByteArray.append("OK\n");
    }
    else
        resultByteArray.append("FAILED\n");
    resultByteArray.append("</root>\n");

    return registered ? CODE_OK : CODE_INTERNAL_ERROR;
}

int QnManualCameraAdditionHandler::executeGet(const QString &path, const QnRequestParamList &params, QByteArray &resultByteArray, QByteArray &contentType)
{
    QString localPath = path;
    while(localPath.endsWith('/'))
        localPath.chop(1);
    QString action = localPath.mid(localPath.lastIndexOf('/') + 1);
    if (action == "search")
        return searchStartAction(params, resultByteArray, contentType);
    else if (action == "status")
        return searchStatusAction(params, resultByteArray, contentType);
    else if (action == "stop")
        return searchStopAction(params, resultByteArray, contentType);
    else if (action == "add")
        return addAction(params, resultByteArray, contentType);
    else {
        resultByteArray.append("<?xml version=\"1.0\"?>\n");
        resultByteArray.append("<root>\n");
        resultByteArray.append("Action not found\n");
        resultByteArray.append("<root>\n");
        return CODE_NOT_FOUND;
    }
}

int QnManualCameraAdditionHandler::executePost(const QString &path, const QnRequestParamList &params, const QByteArray &, QByteArray &result, QByteArray &contentType)
{
    return executeGet(path, params, result, contentType);
}

QString QnManualCameraAdditionHandler::description() const
{
    return 
        "Search or manual add cameras found in the specified range.<BR>\n"
        "<BR><b>api/manualCamera/search</b> - start camera searching"
        "<BR>Param <b>start_ip</b> - first ip address in range."
        "<BR>Param <b>end_ip</b> - end ip address in range. Can be omitted - then only start ip address will be used"
        "<BR>Param <b>port</b> - Port to scan. Can be omitted"
        "<BR>Param <b>user</b> - username for the cameras. Can be omitted.</i>"
        "<BR>Param <b>password</b> - password for the cameras. Can be omitted."
        "<BR><b>Return</b> XML with camera names, manufacturer and urls"
        "<BR>"
        "<BR><b>api/manualCamera/add</b> - manual add camera(s). If several cameras are added, parameters 'ip' and 'manufacturer' must be defined several times"
        "<BR>Param <b>url</b> - camera url returned by scan request."
        "<BR>Param <b>manufacturer</b> - camera manufacturer.</i>"
        "<BR>Param <b>user</b> - username for the cameras. Can be omitted.</i>"
        "<BR>Param <b>password</b> - password for the cameras. Can be omitted."
        "<BR>"
        "<BR><b>api/manualCamera/status</b> - manual addition progress."
        "<BR>Param <b>uuid</b> - process uuid.";
}
