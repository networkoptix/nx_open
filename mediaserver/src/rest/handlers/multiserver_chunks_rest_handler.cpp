#include "multiserver_chunks_rest_handler.h"
#include "recording/time_period.h"
#include "utils/serialization/compressed_time.h"
#include "recording/time_period_list.h"
#include <utils/common/model_functions.h>

int QnMultiserverChunksRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    MultiServerPeriodDataList outputData;
    if (QnFusionRestHandlerDetail::formatFromParams(params) == Qn::CompressedTimePeriods)
    {
        result = QnCompressedTime::serialized(outputData, false);
        contentType = Qn::serializationFormatToHttpContentType(Qn::CompressedTimePeriods);
    }
    else {
        QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType);
    }

    return nx_http::StatusCode::notImplemented;
}
