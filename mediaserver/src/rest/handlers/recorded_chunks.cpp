#include <QFileInfo>
#include "recorded_chunks.h"
#include "recorder/storage_manager.h"
#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "core/resource_managment/resource_pool.h"
#include "utils/common/util.h"
#include "motion/motion_helper.h"
#include "api/serializer/serializer.h"

qint64 QnRecordedChunksHandler::parseDateTime(const QString& dateTime)
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

QRect QnRecordedChunksHandler::deserializeMotionRect(const QString& rectStr)
{
    QList<QString> params = rectStr.split(",");
    if (params.size() != 4)
        return QRect();
    else
        return QRect(params[0].toInt(), params[1].toInt(), params[2].toInt(), params[3].toInt());
}

int QnRecordedChunksHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    qint64 startTime = -1, endTime = 1, detailLevel = -1;
    QList<QnResourcePtr> resList;
    QByteArray errStr;
    QByteArray errStrPhysicalId;
    bool urlFound = false;
    bool useBinary = false;
    QList<QRegion> motionRegions;
    QString callback;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "motionRegions")
        {
            parseRegionList(motionRegions, params[i].second);
        }
        if (params[i].first == "physicalId" || params[i].first == "mac") // use 'mac' name for compatibility with previous client version
        {
            urlFound = true;
            QString physicalId = params[i].second.trimmed();
            QnResourcePtr res = qnResPool->getNetResourceByPhysicalId(physicalId);
            if (res == 0)
                errStrPhysicalId += QByteArray("Resource with physicalId '") + physicalId.toUtf8() + QByteArray("' not found. \n");
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
        else if (params[i].first == "callback")
            callback = params[i].second;
    }
    if (!urlFound)
        errStr += "Parameter physicalId must be provided. \n";
    if (startTime < 0)
        errStr += "Parameter startTime must be provided. \n";
    if (endTime < 0)
        errStr += "Parameter endTime must be provided. \n";
    if (detailLevel < 0)
        errStr += "Parameter detail must be provided. \n";

    if (resList.isEmpty())
        errStr += errStrPhysicalId;

    if (!errStr.isEmpty())
    {
        result.append("<root>\n");
        result.append(errStr);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    }

    QnTimePeriodList periods;
    if (motionRegions.isEmpty())
        periods = qnStorageMan->getRecordedPeriods(resList, startTime, endTime, detailLevel);
    else
        periods = QnMotionHelper::instance()->mathImage(motionRegions, resList, startTime, endTime, detailLevel);
    if (useBinary) {
        result.append("BIN");
        periods.encode(result);
    }
    else {
#if 0 // Roma asked to not remove this wonderful piece of code
        result.append("<recordedTimePeriods xmlns=\"http://www.networkoptix.com/xsd/api/recordedTimePeriods\">\n");
        foreach(QnTimePeriod period, periods)
            result.append(QString("<timePeriod startTime=\"%1\" duration=\"%2\" />\n").arg(period.startTimeMs).arg(period.durationMs));
        result.append("</recordedTimePeriods>\n");
#else
        contentType = "application/json";

        result.append(callback);
        result.append("([");
        int nSize = periods.size();
        for (int n = 0; n < nSize; ++n)
        {
            if (n)
                result.append(", ");

            const QnTimePeriod& period = periods[n];
            result.append("[")
                .append(QByteArray::number(period.startTimeMs)).append(", ").append(QByteArray::number(period.durationMs))
                .append("]");
        }
            
        result.append("]);");
#endif
    }

    return CODE_OK;
}

int QnRecordedChunksHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnRecordedChunksHandler::description(TCPSocket* tcpSocket) const
{
    Q_UNUSED(tcpSocket)
    QString rez;
    rez += "Return recorded chunk info by specified cameras\n";
    rez += "<BR>Param <b>physicalId</b> - camera physicalId. Param can be repeated several times for many cameras.";
    rez += "<BR>Param <b>startTime</b> - Time interval start. Microseconds since 1970 UTC or string in format 'YYYY-MM-DDThh24:mi:ss.zzz'. format is auto detected.";
    rez += "<BR>Param <b>endTime</b> - Time interval end (same format, see above).";
    rez += "<BR>Param <b>motionRect</b> - Match motion on a video by specified rect. Params can be used several times.";
    rez += "<BR>Param <b>detail</b> - Chunk detail level, in microseconds. Time periods/chunks that are shorter than the detail level are discarded. You can use detail level as amount of microseconds per screen pixel.";

    rez += "<BR><b>Return</b> XML - with chunks merged for all cameras. Returned time and duration in microseconds.";
    // rez += getXsdUrl(tcpSocket);
    return rez;
}

int QnXsdHelperHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(params)
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

int QnXsdHelperHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}
