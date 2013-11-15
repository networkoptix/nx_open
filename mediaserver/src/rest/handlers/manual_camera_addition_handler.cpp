#include "manual_camera_addition_handler.h"

#include <QtNetwork/QAuthenticator>

#include "utils/network/tcp_connection_priv.h"
#include "core/resource_managment/resource_discovery_manager.h"
#include "core/resource/resource.h"
#include "core/resource_managment/resource_searcher.h"
#include "core/resource_managment/resource_pool.h"
#include "core/resource/camera_resource.h"

#include <utils/common/json.h>

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

int QnManualCameraAdditionHandler::searchAction(const QnRequestParamList &params, QByteArray &resultByteArray, QByteArray &contentType)
{
    Q_UNUSED(contentType)

    int port = 0;
    QAuthenticator auth;
    auth.setUser("admin");
    auth.setPassword("admin"); // default values

    QString addr1;
    QString addr2;
    QString errStr;

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

    if (addr1.isNull() && errStr.isEmpty())
        errStr = tr("Parameter 'start_ip' missed");

    if (!errStr.isEmpty()) {
        resultByteArray.append("<?xml version=\"1.0\"?>\n");
        resultByteArray.append("<root>\n");
        resultByteArray.append(errStr.toUtf8());
        resultByteArray.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    if (addr2 == addr1)
        addr2.clear();
    //if (addr2.isNull())
    //    addr2 = addr1;

    QnResourceList resources = QnResourceDiscoveryManager::instance()->findResources(addr1, addr2, auth, port);

    resultByteArray.append("<?xml version=\"1.0\"?>\n");
    //TODO: #GDM morph to json? whats with compatibility?

    resultByteArray.append("<root>\n");
    {
        resultByteArray.append("<query>\n");
        {
            resultByteArray.append(QString("<start_ip>%1</start_ip>\n").arg(addr1));
            resultByteArray.append(QString("<end_ip>%1</end_ip>\n").arg(addr2));
            resultByteArray.append(QString("<port>%1</port>\n").arg(port));
            resultByteArray.append(QString("<user>%1</user>\n").arg(auth.user()));
            resultByteArray.append(QString("<password>%1</password>\n").arg(auth.password()));
        }
        resultByteArray.append("</query>\n");

        resultByteArray.append("<reply>\n");
        {
            foreach(const QnResourcePtr &resource, resources){
                QnResourceTypePtr resourceType = qnResTypePool->getResourceType(resource->getTypeId());

                resultByteArray.append("<resource>\n");
                resultByteArray.append(QString("<name>%1</name>\n").arg(resource->getName()));
                resultByteArray.append(QString("<url>%1</url>\n").arg(resource->getUrl()));
                resultByteArray.append(QString("<manufacturer>%1</manufacturer>\n").arg(resourceType->getName()));

                bool existResource = false;
                if (qnResPool->hasSuchResource(resource->getUniqueId())) { 
                    existResource = true; // already in resource pool 
                }
                else {
                    // For onvif uniqID may be different. Some GUID in pool and macAddress after manual adding. So, do addition cheking for IP address
                    QnNetworkResourcePtr netRes = resource.dynamicCast<QnNetworkResource>();
                    if (netRes) {
                        QnNetworkResourceList existResList = qnResPool->getAllNetResourceByHostAddress(netRes->getHostAddress());
                        foreach(QnNetworkResourcePtr existRes, existResList) 
                        {
                            if (existRes->getTypeId() != netRes->getTypeId())
                                existResource = true; // camera found by different drivers

                            QnVirtualCameraResourcePtr existCam = existRes.dynamicCast<QnVirtualCameraResource>();
                            if (!existCam->isManuallyAdded())
                                existResource = true; // block manual and auto add in same time
                        }
                    }
                }
                resultByteArray.append(QString("<exists>%1</exists>\n").arg(existResource ? 1 : 0));

                resultByteArray.append("</resource>\n");
            }
        }
        resultByteArray.append("</reply>\n");
    }
    resultByteArray.append("</root>\n");

    return CODE_OK;
}

int QnManualCameraAdditionHandler::searchStatusAction(const QnRequestParamList &params, QByteArray &resultByteArray, QByteArray &contentType) {
    contentType = "application/json";

    QUuid processGuid;
    for (int i = 0; i < params.size(); ++i)
    {
        QPair<QString, QString> param = params[i];
        if (param.first == "guid")
            processGuid = QUuid(param.second);
    }

    if (processGuid.isNull())
        return CODE_INVALID_PARAMETER;

    QnManualSearchStatus status;
    if (!QnResourceDiscoveryManager::instance()->getSearchStatus(processGuid, status))
        return CODE_NOT_FOUND;

    QJson::serialize(status, &resultByteArray);
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
        return searchAction(params, resultByteArray, contentType);
    else if (action == "add")
        return addAction(params, resultByteArray, contentType);
    else if (action == "status")
        return searchStatusAction(params, resultByteArray, contentType);
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
        "<BR>Param <b>guid</b> - process uuid.";
}
