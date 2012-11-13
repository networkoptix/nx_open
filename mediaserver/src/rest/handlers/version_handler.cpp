#include "version_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "version.h"

int QnGetVersionHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    int offset = currentTimeZone();
    QString offsetStr = QString::number(offset);
    if (offset >= 0)
        offsetStr = QLatin1String("+") + offsetStr;

    QString dateStr = qnSyncTime->currentDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ss.zzzZ");
    result.append(QString("<root><version>%1</version><name>%2</name></root>\n").arg(VER_PRODUCTVERSION_STR).arg(VER_FILEDESCRIPTION_STR).toUtf8());
    return CODE_OK;
}

int QnGetVersionHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnGetVersionHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Returns server version";
    return rez;
}
