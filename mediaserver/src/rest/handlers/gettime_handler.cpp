#include "gettime_handler.h"
#include "utils/network/tcp_connection_priv.h"
#include "utils/common/synctime.h"

QnGetTimeHandler::QnGetTimeHandler()
{

}

int QnGetTimeHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    QDateTime dt = qnSyncTime->currentDateTime();

    QDateTime dt1 = dt;
    QDateTime dt2 = dt1.toUTC();
    dt1.setTimeSpec(Qt::UTC);
    int offset = dt2.secsTo(dt1);
    QString offsetStr = QString::number(offset);
    if (offset >= 0)
        offsetStr = QLatin1String("+") + offsetStr;

    QString dateStr = dt.toUTC().toString("yyyy-MM-ddThh:mm:ss.zzzZ");
    result.append(QString("<time clock=\"%1\" timezone=\"%2\"/>\n").arg(dateStr).arg(offsetStr).toUtf8());
    return CODE_OK;
}

int QnGetTimeHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnGetTimeHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Returns server UTC time and time zone";
    return rez;
}
