#include <QFileInfo>
#include <QAuthenticator>
#include "manual_camera_addition.h"
#include "utils/network/tcp_connection_priv.h"
#include "core/resourcemanagment/resource_discovery_manager.h"

QnManualCameraAdditionHandler::QnManualCameraAdditionHandler()
{

}

int QnManualCameraAdditionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)

    if (params.size() < 4 && 0 )
    {
        // to few params
        result.append(QByteArray("To few params"));
        return CODE_INVALID_PARAMETER;
    }

    /*
    QHostAddress addr1 = QHostAddress(params[0].second);
    QHostAddress addr2 = QHostAddress(params[1].second);

    QAuthenticator auth;
    auth.setUser(QHostAddress(params[2].second));
    auth.setPassword(QHostAddress(params[3].second))
    /**/

    QHostAddress addr1 = QHostAddress("192.168.0.0");
    QHostAddress addr2 = QHostAddress("192.168.0.255");

    QAuthenticator auth;
    auth.setUser("admin");
    auth.setPassword("admin");


    QnResourceDiscoveryManager::instance().findResources(addr1, addr2, auth);

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

    rez += "ya schitau chto inoplanetane est'. ih ne mojet ne bit'\n";
    return rez;
}
