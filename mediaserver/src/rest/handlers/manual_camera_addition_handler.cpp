#include "manual_camera_addition_handler.h"

#include <QtCore/QFileInfo>
#include <QtNetwork/QAuthenticator>

#include "utils/network/tcp_connection_priv.h"
#include "core/resource_managment/resource_discovery_manager.h"
#include "core/resource/resource.h"
#include "core/resource_managment/resource_searcher.h"

QnManualCameraAdditionHandler::QnManualCameraAdditionHandler()
{

}

int QnManualCameraAdditionHandler::searchAction(const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
{
    Q_UNUSED(contentType)

    int port = 0;
    QAuthenticator auth;
    auth.setUser("admin");
    auth.setPassword("admin"); // default values

    QHostAddress addr1;
    QHostAddress addr2;

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

    if (addr1.isNull()) {
        resultByteArray.append("<?xml version=\"1.0\"?>\n");
        resultByteArray.append("<root>\n");
        resultByteArray.append(QByteArray("Parameter 'start_ip' missed" + addr1.toString().toUtf8()));
        resultByteArray.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }
    if (addr2.isNull())
        addr2 = addr1;

    QnResourceList resources = QnResourceDiscoveryManager::instance().findResources(addr1, addr2, auth, port);

    resultByteArray.append("<?xml version=\"1.0\"?>\n");
    // TODO #gdm implement simple XML-builder, will be quite useful

    resultByteArray.append("<root>\n");
    {
        resultByteArray.append("<query>\n");
        {
            resultByteArray.append(QString("<start_ip>%1</start_ip>\n").arg(addr1.toString()));
            resultByteArray.append(QString("<end_ip>%1</end_ip>\n").arg(addr2.toString()));
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
                resultByteArray.append("</resource>\n");
            }
        }
        resultByteArray.append("</reply>\n");
    }
    resultByteArray.append("</root>\n");

    return CODE_OK;
}

int QnManualCameraAdditionHandler::addAction(const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
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
    bool registered = QnResourceDiscoveryManager::instance().registerManualCameras(cameras);
    if (registered) {
        QnResourceDiscoveryManager::instance().processManualAddedResources();
        resultByteArray.append("OK\n");
    }
    else
        resultByteArray.append("FAILED\n");
    resultByteArray.append("</root>\n");

    return registered ? CODE_OK : CODE_INTERNAL_ERROR;
}

int QnManualCameraAdditionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
{
    Q_UNUSED(contentType)

    QString localPath = path;
    while(localPath.endsWith('/'))
        localPath.chop(1);
    QString action = localPath.mid(localPath.lastIndexOf('/') + 1);
    if (action == "search")
        return searchAction(params, resultByteArray, contentType);
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

int QnManualCameraAdditionHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
        return executeGet(path, params, result, contentType);
}

QString QnManualCameraAdditionHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
        QString rez;
    rez += "Search or manual add cameras found in the specified range.<BR>\n";
    rez += "<BR><b>api/manualCamera/search</b> - start camera searching";
    rez += "<BR>Param <b>start_ip</b> - first ip address in range.";
    rez += "<BR>Param <b>end_ip</b> - end ip address in range. Can be omitted - then only start ip address will be used";
    rez += "<BR>Param <b>port</b> - Port to scan. Can be omitted";
    rez += "<BR>Param <b>user</b> - username for the cameras. Can be omitted.</i>";
    rez += "<BR>Param <b>password</b> - password for the cameras. Can be omitted.";
    rez += "<BR><b>Return</b> XML with camera names, manufacturer and urls";

    rez += "<BR>";

    rez += "<BR><b>api/manualCamera/add</b> - manual add camera(s). If several cameras are added, parameters 'ip' and 'manufacturer' must be defined several times";
    rez += "<BR>Param <b>url</b> - camera url returned by scan request.";
    rez += "<BR>Param <b>manufacturer</b> - camera manufacturer.</i>";
    rez += "<BR>Param <b>user</b> - username for the cameras. Can be omitted.</i>";
    rez += "<BR>Param <b>password</b> - password for the cameras. Can be omitted.";

    return rez;
}
