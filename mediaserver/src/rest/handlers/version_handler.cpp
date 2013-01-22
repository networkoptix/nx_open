#include "version_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "version.h"

int QnVersionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    result.append(QString("<root><engineVersion>%1</engineVersion><revision>%2</revision></root>\n").arg(QN_ENGINE_VERSION).arg(QN_APPLICATION_REVISION).toUtf8());
    return CODE_OK;
}

int QnVersionHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnVersionHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Returns server version";
    return rez;
}
