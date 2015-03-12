#include <QUrlQuery>
#include <QWaitCondition>

#include "camera_history_rest_handler.h"

ec2::ApiCameraHistoryDetailData QnCameraHistoryRestHandler::buildHistoryData(const MultiServerPeriodDataList& chunks)
{
    // todo: implement me
    ec2::ApiCameraHistoryDetailData result;
    return result;
}

int QnCameraHistoryRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    QnChunksRequestData request = QnChunksRequestData::fromParams(params);
    MultiServerPeriodDataList chunks = QnMultiserverChunksRestHandler::loadDataSync(request);
    ec2::ApiCameraHistoryDetailData outputData = buildHistoryData(chunks);
    QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType);
    return nx_http::StatusCode::ok;
}
