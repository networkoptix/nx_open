#include <QFileInfo>
#include <QAuthenticator>
#include "manual_camera_addition.h"
#include "utils/network/tcp_connection_priv.h"
#include "core/resourcemanagment/resource_discovery_manager.h"

QnManualCameraAdditionHandler::QnManualCameraAdditionHandler()
{

}

int QnManualCameraAdditionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& resultByteArray, QByteArray& contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(contentType)

    QString startAddr;
    QString endAddr;
    QString user;
    QString password;

    for (int i = 0; i < params.size(); ++i){
        QPair<QString, QString> param = params[i];
        if (param.first == "start")
            startAddr = param.second;
        else
        if (param.first == "end")
            endAddr = param.second;
        else
        if (param.first == "user")
            user = param.second;
        else
        if (param.first == "password")
            password = param.second;
    }

    QHostAddress addr1 = QHostAddress(startAddr);
    if (addr1.isNull()) {
        resultByteArray.append(QByteArray("Invalid start parameter" + addr1.toString().toUtf8()));
        return CODE_INVALID_PARAMETER;
    }

    QHostAddress addr2;
    if (endAddr.length() > 0)
        addr2 = endAddr;
    else
        addr2 = addr1;

    QAuthenticator auth;
    if (user.length() > 0)
        auth.setUser(user);
    else
        auth.setUser("admin");

    if (password.length() > 0)
        auth.setPassword(password);
    else
        auth.setPassword("admin");

    QnResourceList resources = QnResourceDiscoveryManager::instance().findResources(addr1, addr2, auth);

    QString result;
    result.append("<?xml version=\"1.0\"?>\n");
    result.append("<root>\n");

    foreach(const QnResourcePtr &resource, resources){
        result.append("<resource>\n");
        result.append(QString("<name>%1</name>\n").arg(resource->getName()));
        result.append(QString("<url>%1</url>\n").arg(resource->getUrl()));
        result.append("</resource>\n");
    }

    for (int i = 0; i < 5; i++){
        result.append("<resource>\n");
        result.append(QString("<name>%1</name>\n").arg("dummy " + QString::number(i)));
        result.append(QString("<url>%1</url>\n").arg("dummy ip" + QString::number(i)));
        result.append("</resource>\n");
    }

    result.append("</root>\n");

    resultByteArray = result.toUtf8();

    return CODE_OK;
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
    rez += "Return cameras info found in the specified range\n";
    rez += "<BR>Param <b>start</b> - first ip address in range.";
    rez += "<BR>Param <b>end</b> - end ip address in range. Can be omitted - then only start ip address will be used";
    rez += "<BR>Param <b>user</b> - username for the cameras. Can be omitted. Default value: <i>admin</i>";
    rez += "<BR>Param <b>password</b> - password for the cameras. Can be omitted. Default value: <i>admin</i>";

    rez += "<BR><b>Return</b> XML with camera names and urls";
    return rez;
}
