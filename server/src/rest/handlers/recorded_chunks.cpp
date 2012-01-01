#include <QFileInfo>
#include "recorded_chunks.h"
#include "recorder/storage_manager.h"
#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "core/resourcemanagment/resource_pool.h"
#include "utils/common/util.h"
#include "motion/motion_helper.h"

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
            tmpDateTime = tmpDateTime.addMSecs(dateTimeParts[1].toInt()/1000);
        return tmpDateTime.toMSecsSinceEpoch() * 1000;
    }
    else
        return dateTime.toLongLong();
}

QRect QnRecordedChunkListHandler::deserializeMotionRect(const QString& rectStr)
{
    QList<QString> params = rectStr.split(",");
    if (params.size() != 4)
        return QRect();
    else
        return QRect(params[0].toInt(), params[1].toInt(), params[2].toInt(), params[3].toInt());
}

int QnRecordedChunkListHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result)
{
    qint64 startTime = -1, endTime = 1, detailLevel = -1;
    QList<QnResourcePtr> resList;
    QByteArray errStr;
    QByteArray errStrMac;
    bool urlFound = false;
    bool useBinary = false;
    QRegion motionRegion;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "motionRect")
        {
            QRect tmp = deserializeMotionRect(params[i].second);
            if (tmp.width() > 0 && tmp.height() > 0)
                motionRegion += tmp;
        }
        if (params[i].first == "mac")
        {
            urlFound = true;
            QString mac = params[i].second.trimmed();
            QnResourcePtr res = qnResPool->getNetResourceByMac(mac);
            if (res == 0)
                errStrMac += QByteArray("Resource with mac '") + mac.toUtf8() + QByteArray("' not found. \n");
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
        else if (params[i].first == "format")
            useBinary = params[i].second == "bin";
    }
    if (!urlFound)
        errStr += "Parameter mac must be provided. \n";
    if (startTime < 0)
        errStr += "Parameter startTime must be provided. \n";
    if (endTime < 0)
        errStr += "Parameter endTime must be provided. \n";
    if (detailLevel < 0)
        errStr += "Parameter detail must be provided. \n";

    if (resList.isEmpty())
        errStr += errStrMac;

    if (!errStr.isEmpty())
    {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    QnTimePeriodList periods;
    if (motionRegion.isEmpty())
        periods = qnStorageMan->getRecordedPeriods(resList, startTime, endTime, detailLevel);
    else
        periods = QnMotionHelper::instance()->mathImage(motionRegion, resList, startTime, endTime, detailLevel);
    if (useBinary) {
        foreach(QnTimePeriod period, periods)
        {
            qint64 start = htonll(period.startTimeMs);
            qint64 duration = htonll(period.durationMs);
            result.append(((const char*) &start)+2, 6);
            result.append(((const char*) &duration)+2, 6);
        }
    }
    else {
        result.append("<recordedTimePeriods xmlns=\"http://www.networkoptix.com/xsd/api/recordedTimePeriods\">\n");
        foreach(QnTimePeriod period, periods)
            result.append(QString("<timePeriod startTime=\"%1\" duration=\"%2\" />\n").arg(period.startTimeMs).arg(period.durationMs));
        result.append("</recordedTimePeriods>\n");
    }

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
    rez += "<BR>Param <b>startTime</b> - Time interval start. Microseconds since 1970 UTC or string in format 'YYYY-MM-DDThh24:mi:ss.zzz'. format is auto detected.";
    rez += "<BR>Param <b>endTime</b> - Time interval end (same format, see above).";
    rez += "<BR>Param <b>motionRect</b> - Match motion on a video by specified rect. Params can be used several times.";
    rez += "<BR>Param <b>detail</b> - Chunk detail level, in microseconds. Time periods/chunks that are shorter than the detail level are discarded. You can use detail level as amount of microseconds per screen pixel.";

    rez += "<BR><b>Return</b> XML</b> - with chunks merged for all cameras. Returned time and duration in microseconds. <a href=\"";
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
