#include "ping_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "serverutil.h"

QnRestPingHandler::QnRestPingHandler()
{

}

int QnRestPingHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    Q_UNUSED(params)
    Q_UNUSED(contentType)

    result.append(QString("<pong>%1</pong>\n").arg(serverGuid()).toUtf8());
    return CODE_OK;
}

int QnRestPingHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnRestPingHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Returns server ping message";
    return rez;
}
