#include "time_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"

QnTimeHandler::QnTimeHandler()
{

}

int QnTimeHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    int offset = currentTimeZone();
    QString offsetStr = QString::number(offset);
    if (offset >= 0)
        offsetStr = QLatin1String("+") + offsetStr;

    QString dateStr = qnSyncTime->currentDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ss.zzzZ");
    result.append(QString("<time><clock>%1</clock><utcOffset>%2</utcOffset></time>\n").arg(dateStr).arg(offsetStr).toUtf8());
    return CODE_OK;
}

int QnTimeHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnTimeHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Returns server UTC time and time zone";
    return rez;
}
