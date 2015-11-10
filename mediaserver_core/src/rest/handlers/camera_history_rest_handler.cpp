#include <QUrlQuery>
#include <QWaitCondition>

#include <api/helpers/chunks_request_data.h>

#include "camera_history_rest_handler.h"
#include "core/resource/camera_resource.h"

ec2::ApiCameraHistoryItemDataList QnCameraHistoryRestHandler::buildHistoryData(const MultiServerPeriodDataList& chunks)
{
    ec2::ApiCameraHistoryItemDataList result;
    std::vector<int> scanPos;
    scanPos.resize(chunks.size());
    qint64 currentTime = 0;
    int prevIndex = -1;    
    while(1)
    {
        int index = -1;
        qint64 minNextTime = INT64_MAX;
        int nextIndex = -1;
        for (int i = 0; i < scanPos.size(); ++i) 
        {
            int& pos = scanPos[i];
            const QnTimePeriodList& periods = chunks[i].periods;
            for (;pos < periods.size() && periods[pos].endTimeMs() <= currentTime; pos++);
            if (pos < periods.size()) {
                if (periods[pos].contains(currentTime)) {
                    index = i; // exact match
                    break;
                }
                else if (periods[pos].startTimeMs < minNextTime) {
                    minNextTime = periods[pos].startTimeMs;
                    nextIndex = i;
                }
            }
        }
        if (index == -1) {
            index = nextIndex; // data hole. get next min time after hole
            currentTime = minNextTime;
        }
        if (index == -1)
            break; // end of data reached

        const QnTimePeriod& curPeriod = chunks[index].periods[scanPos[index]];
        scanPos[index]++;
        if (index == prevIndex)
            continue; // ignore same server (no changes)

        result.push_back(ec2::ApiCameraHistoryItemData(chunks[index].guid, currentTime));
        currentTime = curPeriod.endTimeMs();
        prevIndex = index;
    }

    return result;
}

int QnCameraHistoryRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    QnChunksRequestData request = QnChunksRequestData::fromParams(params);
    if (!request.isValid())
        return nx_http::StatusCode::badRequest;

    ec2::ApiCameraHistoryDataList outputData;
    for (const auto& camera: request.resList)
    {
        QnChunksRequestData updatedRequest = request;
        updatedRequest.resList.clear();
        updatedRequest.resList.push_back(camera);
        MultiServerPeriodDataList chunks = QnMultiserverChunksRestHandler::loadDataSync(updatedRequest, owner);
        ec2::ApiCameraHistoryData outputRecord;
        outputRecord.cameraId = request.resList.first()->getId();
        outputRecord.items = buildHistoryData(chunks);
        outputData.push_back(std::move(outputRecord));
    }

    QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType);
    return nx_http::StatusCode::ok;
}
