#include "recorded_chunks_rest_handler.h"

#include <api/helpers/chunks_request_data.h>

#include "recorder/storage_manager.h"

#include "core/resource_management/resource_pool.h"
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <nx/network/http/custom_headers.h>

#include "motion/motion_helper.h"

#include <rest/helpers/chunks_request_helper.h>

#include <utils/fs/file.h>
#include "network/tcp_connection_priv.h"
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

int QnRecordedChunksRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    QByteArray errStr;
    QnChunksRequestData request = QnChunksRequestData::fromParams(owner->commonModule()->resourcePool(), params);
    QString callback = params.value("callback");

    if (request.resList.isEmpty())
        errStr += "At least one parameter 'id' must be provided.\n";
    if (request.startTimeMs < 0)
        errStr += "Parameter 'startTime' must be provided.\n";
    if (request.endTimeMs < 0)
        errStr += "Parameter 'endTime' must be provided.\n";
    if (request.detailLevel < std::chrono::milliseconds::zero())
        errStr += "Parameter 'detail' must be provided.\n";
    if (request.format == Qn::UnsupportedFormat)
        errStr += "Parameter 'format' must be provided.\n";

    auto errLog =
        [&](const QString& errText)
        {
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
            periods.encode(result);
            break;

        case ChunkFormat_XML:
            result.append(
                "<recordedTimePeriods xmlns=\"http://www.networkoptix.com/xsd/api/recordedTimePeriods\">\n");
            for (const QnTimePeriod& period: periods)
            {
                result.append(QString("<timePeriod startTime=\"%1\" duration=\"%2\" />\n")
                    .arg(period.startTimeMs).arg(period.durationMs));
            }
            result.append("</recordedTimePeriods>\n");
            break;

        case ChunkFormat_Text:
            result.append("<root>\n");
            for (const QnTimePeriod& period: periods)
            {
                result.append("<chunk>");
                result.append(QDateTime::fromMSecsSinceEpoch(period.startTimeMs).toString(
                    QLatin1String("dd-MM-yyyy hh:mm:ss.zzz")));
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
                    .append(QByteArray::number(period.startTimeMs)).append(", ")
                    .append(QByteArray::number(period.durationMs))
                    .append("]");
            }
            result.append("]");
    }

    return CODE_OK;
}

int QnRecordedChunksRestHandler::executePost(
    const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/, QByteArray& result, QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}
