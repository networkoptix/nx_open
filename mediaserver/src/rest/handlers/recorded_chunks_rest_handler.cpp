
#include "recorded_chunks_handler.h"

#include "recorder/storage_manager.h"
#include "utils/network/tcp_connection_priv.h"
#include "rest/server/rest_server.h"
#include "core/resource_management/resource_pool.h"
#include "utils/common/util.h"
#include <utils/fs/file.h>
#include "motion/motion_helper.h"
#include "api/serializer/serializer.h"


QRect QnRecordedChunksRestHandler::deserializeMotionRect(const QString& rectStr)
{
    QList<QString> params = rectStr.split(",");
    if (params.size() != 4)
        return QRect();
    else
        return QRect(params[0].toInt(), params[1].toInt(), params[2].toInt(), params[3].toInt());
}

int QnRecordedChunksRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(path)
    qint64 startTime = -1, endTime = 1, detailLevel = -1;
    QList<QnResourcePtr> resList;
    QByteArray errStr;
    QByteArray errStrPhysicalId;
    bool urlFound = false;
    
    ChunkFormat format = ChunkFormat_Unknown;
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
            startTime = parseDateTime(params[i].second.toUtf8());
        }
        else if (params[i].first == "endTime") {
            endTime = parseDateTime(params[i].second.toUtf8());
        }
        else if (params[i].first == "detail")
            detailLevel = params[i].second.toLongLong();
        else if (params[i].first == "format") 
        {
            if (params[i].second == "bin")
                format = ChunkFormat_Binary;
            else if (params[i].second == "xml")
                format = ChunkFormat_XML;
            else if (params[i].second == "txt")
                format = ChunkFormat_Text;
            else
                format = ChunkFormat_Json;
        }
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
    
    switch(format) 
    {
        case ChunkFormat_Binary:
            result.append("BIN");
            periods.encode(result);
            break;
        case ChunkFormat_XML:
            result.append("<recordedTimePeriods xmlns=\"http://www.networkoptix.com/xsd/api/recordedTimePeriods\">\n");
            foreach(QnTimePeriod period, periods)
                result.append(QString("<timePeriod startTime=\"%1\" duration=\"%2\" />\n").arg(period.startTimeMs).arg(period.durationMs));
            result.append("</recordedTimePeriods>\n");
            break;
        case ChunkFormat_Text:
            result.append("<root>\n");
            foreach(QnTimePeriod period, periods) {
                result.append("<chunk>");
                result.append(QDateTime::fromMSecsSinceEpoch(period.startTimeMs).toString(QLatin1String("dd-MM-yyyy hh:mm:ss.zzz")));
                result.append("    ");
                result.append(QString::number(period.durationMs));
                result.append("</chunk>\n");
            }
            result.append("</root>\n");
            break;
        case ChunkFormat_Json:
        default:
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
    }

    return CODE_OK;
}

int QnRecordedChunksRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType)
{
    Q_UNUSED(body)
    return executeGet(path, params, result, contentType);
}

QString QnRecordedChunksRestHandler::description() const
{
    return 
        "Return recorded chunk info by specified cameras\n"
        "<BR>Param <b>physicalId</b> - camera physicalId. Param can be repeated several times for many cameras."
        "<BR>Param <b>startTime</b> - Time interval start. Microseconds since 1970 UTC or string in format 'YYYY-MM-DDThh24:mi:ss.zzz'. format is auto detected."
        "<BR>Param <b>endTime</b> - Time interval end (same format, see above)."
        "<BR>Param <b>motionRegions</b> - Match motion on a video by specified rect. Params can be used several times."
        "<BR>Param <b>format</b> - Optional. Data format. Allowed values: 'json', 'xml', 'txt', 'bin'. Default value 'json'"
        "<BR>Param <b>detail</b> - Chunk detail level, in microseconds. Time periods/chunks that are shorter than the detail level are discarded. You can use detail level as amount of microseconds per screen pixel."
        "<BR><b>Return</b> XML - with chunks merged for all cameras. Returned time and duration in microseconds.";
}
