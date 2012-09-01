#include <QFileInfo>
#include "manual_camera_addition.h"
#include "utils/network/tcp_connection_priv.h"

QnManualCameraAdditionHandler::QnManualCameraAdditionHandler()
{

}

int QnManualCameraAdditionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)

    if (params.size() < 4)
    {
        // to few params
        result.append(QByteArray("To few params"));
        return CODE_INVALID_PARAMETER;
    }



    //QnResourceDiscoveryManager::instance().findResources()

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
