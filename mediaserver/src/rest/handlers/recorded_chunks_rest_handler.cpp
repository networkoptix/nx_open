#include "recorded_chunks_rest_handler.h"

#include <api/helpers/chunks_request_data.h>

#include "recorder/storage_manager.h"

#include "core/resource_management/resource_pool.h"
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>

#include "motion/motion_helper.h"

#include <rest/helpers/chunks_request_helper.h>

#include "utils/common/util.h"
#include <utils/fs/file.h>
#include "utils/network/tcp_connection_priv.h"
#include <utils/serialization/json.h>
#include <utils/serialization/json_functions.h>

int QnRecordedChunksRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    
    QByteArray errStr;
    QnChunksRequestData request = QnChunksRequestData::fromParams(params);
    QString callback = params.value("callback");
    
    if (request.resList.isEmpty())
        errStr += "Parameter physicalId must be provided. \n";
    if (request.startTimeMs < 0)
        errStr += "Parameter startTime must be provided. \n";
    if (request.endTimeMs < 0)
        errStr += "Parameter endTime must be provided. \n";
    if (request.detailLevel < 0)
        errStr += "Parameter detail must be provided. \n";
    if (request.format == Qn::UnsupportedFormat)
        errStr += "Parameter format must be provided. \n";

    auto errLog = [&](const QString &errText) {
        result.append("<root>\n");
        result.append(errText);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    };

    if (!errStr.isEmpty())
        return errLog(errStr);
    
    QnTimePeriodList periods = QnChunksRequestHelper::load(request);

    switch(request.format) 
    {
        case ChunkFormat_Binary:
            result.append("BIN");
            periods.encode(result, false);
            break;
        case ChunkFormat_BinaryIntersected:
            result.append("BII");
            periods.encode(result, true);
            break;
        case ChunkFormat_XML:
            result.append("<recordedTimePeriods xmlns=\"http://www.networkoptix.com/xsd/api/recordedTimePeriods\">\n");
            for(const QnTimePeriod& period: periods)
                result.append(QString("<timePeriod startTime=\"%1\" duration=\"%2\" />\n").arg(period.startTimeMs).arg(period.durationMs));
            result.append("</recordedTimePeriods>\n");
            break;
        case ChunkFormat_Text:
            result.append("<root>\n");
            for(const QnTimePeriod& period: periods) {
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
            result.append("[");
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
                
            result.append("]");
            break;
    }

    return CODE_OK;
}

int QnRecordedChunksRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType*/, QByteArray& result, 
                                             QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}

