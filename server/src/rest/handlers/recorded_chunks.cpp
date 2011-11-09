#include <QFileInfo>
#include "recorded_chunks.h"
#include "recording/storage_manager.h"
#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "core/resourcemanagment/resource_pool.h"

QString QnRestXsdHelpHandler::getXsdUrl(TCPSocket* tcpSocket) const
{
    QString rez;
    rez += QString("http://") + tcpSocket->getLocalAddress() + QString(":") + QString::number(tcpSocket->getLocalPort());
    rez += "/xsd";
    rez += m_path.mid(m_path.lastIndexOf('/'));
    rez += ".xsd";
    rez += "\"> XSD scheme </a>";
    return rez;
}

qint64 QnRecordedChunkListHandler::parseDateTime(const QString& dateTime)
{
    if (dateTime.contains('T') || dateTime.contains('-'))
    {
        QStringList dateTimeParts = dateTime.split('.');
        QDateTime tmpDateTime = QDateTime::fromString(dateTimeParts[0], Qt::ISODate);
        if (dateTimeParts.size() > 1)
            tmpDateTime = tmpDateTime.addMSecs(dateTimeParts[1].toInt());
        return tmpDateTime.toMSecsSinceEpoch() * 1000;
    }
    else
        return dateTime.toLongLong() * 1000;
}

int QnRecordedChunkListHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result)
{
    qint64 startTime = -1, endTime = 1, detailLevel = -1;
    QList<QnResourcePtr> resList;
    QByteArray errStr;
    bool urlFound = false;
    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "mac")
        {
            urlFound = true;
            QString mac = params[i].second.trimmed();
            QnResourcePtr res = qnResPool->getNetResourceByMac(mac);
            if (res == 0)
                errStr += QByteArray("Resource with mac '") + mac.toUtf8() + QByteArray("' not found. \n");
            else 
                resList << res;
        }
        else if (params[i].first == "startTime") 
        {
            startTime = parseDateTime(params[i].second);
        }
        else if (params[i].first == "endTime") {
            endTime = parseDateTime(params[i].second);
        }
        else if (params[i].first == "detail")
            detailLevel = params[i].second.toLongLong();
    }
    if (!urlFound)
        errStr += "Parameter mac must be provided. \n";
    if (startTime < 0)
        errStr += "Parameter startTime must be provided. \n";
    if (endTime < 0)
        errStr += "Parameter endTime must be provided. \n";
    if (detailLevel < 0)
        errStr += "Parameter detail must be provided. \n";

    if (!errStr.isEmpty())
    {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    QnTimePeriodList periods = qnStorageMan->getRecordedPeriods(resList, startTime, endTime, detailLevel);

    result.append("<recordedTimePeriods xmlns=\"http://www.networkoptix.com/xsd/api/recordedTimePeriods\">\n");

    foreach(QnTimePeriod period, periods)
    {
        qint64 duration = period.duration;
        if (duration == -1)
            duration = QDateTime::currentDateTime().toMSecsSinceEpoch()*1000ll - period.startTime;
        result.append(QString("<timePeriod startTime=\"%1\" duration=\"%2\" />\n").arg(period.startTime).arg(duration));
    }
    result.append("</recordedTimePeriods>\n");

    return CODE_OK;
}

int QnRecordedChunkListHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result)
{
    return executeGet(path, params, result);
}

QString QnRecordedChunkListHandler::description(TCPSocket* tcpSocket) const
{
    QString rez;
    rez += "Return recorded chunk info by specified cameras\n";
    rez += "<BR>Param <b>mac</b> - camera mac. Param can be repeated several times for many cameras.";
    rez += "<BR>Param <b>startTime</b> - Time interval start. ms since 1970 UTC or string at format 'YYYY-MM-DDThh24:mi:ss.ms', format auto detected)";
    rez += "<BR>Param <b>endTime</b> - Time interval end (same format, see above)";
    rez += "<BR>Param <b>detail</b> - Chunk detail level. Time periods/chunks less than detal level is discarded. You can use detail level as amount of mks per screen pixel";

    rez += "<BR><b>Return</b> XML</b> - with chunks merged by all cameras. <a href=\"";
    rez += getXsdUrl(tcpSocket);
    return rez;
}

int QnXsdHelperHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result)
{
    //QString method = path.mid(path.lastIndexOf('/')+1);
    QString method = QFileInfo(path).baseName();

    QFile f(QString(":/xsd/api/") + method);
    f.open(QFile::ReadOnly);
    QByteArray data = f.readAll();
    if (!data.isEmpty()) 
    {
        result.append(data);
        return CODE_OK;
    }
    else {
        result.append("<root>XSD scheme not found </root>");
        return CODE_NOT_FOUND;
    }
}

int QnXsdHelperHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result)
{
    return executeGet(path, params, result);
}
