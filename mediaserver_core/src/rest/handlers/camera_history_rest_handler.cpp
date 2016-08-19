#include <QUrlQuery>
#include <QWaitCondition>

#include <vector>
#include <algorithm>

#include <api/helpers/chunks_request_data.h>

#include "camera_history_rest_handler.h"
#include "core/resource/camera_resource.h"

namespace {

struct CandidateTimePeriod
{
    QnUuid serverId;
    QnTimePeriod period;
    int index;
    CandidateTimePeriod(const QnUuid& serverId, const QnTimePeriod& period, int index):
        serverId(serverId),
        period(period),
        index(index)
    {}
    CandidateTimePeriod(): index(-1) {}
    bool isNull() const { return period.isNull(); }
    operator bool() const { return !isNull();  }
};

CandidateTimePeriod findMinStartTimePeriod(
    std::vector<int>& scanPos,
    const MultiServerPeriodDataList& chunks)
{
    CandidateTimePeriod result;
    qint64 minTime = std::numeric_limits<qint64>::max();

    for (int i = 0; i < scanPos.size(); ++i)
    {
        if (scanPos[i] >= chunks[i].periods.size())
            continue; //< no periods left

        const auto& period = chunks[i].periods[scanPos[i]];
        if (period.startTimeMs < minTime ||
           (period.startTimeMs == minTime && period.endTimeMs() > result.period.endTimeMs()))
        {
            result = CandidateTimePeriod(chunks[i].guid, period, i);
            minTime = period.startTimeMs;
        }
    }
    if (!result.isNull())
        ++scanPos[result.index];
    return result;
}

} // namespace

ec2::ApiCameraHistoryItemDataList QnCameraHistoryRestHandler::buildHistoryData(const MultiServerPeriodDataList& chunks)
{
    ec2::ApiCameraHistoryItemDataList result;
    std::vector<int> scanPos(chunks.size());

    CandidateTimePeriod prevPeriod;
    while (auto candidate = findMinStartTimePeriod(scanPos, chunks))
    {
        if (prevPeriod.period.contains(candidate.period))
            continue; //< no time advance
        if (prevPeriod.serverId != candidate.serverId)
            result.push_back(ec2::ApiCameraHistoryItemData(candidate.serverId, candidate.period.startTimeMs));
        prevPeriod = candidate;
    }

    return result;
}

int QnCameraHistoryRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    QnChunksRequestData request = QnChunksRequestData::fromParams(params);
    if (!request.isValid())
        return nx_http::StatusCode::badRequest;

    ec2::ApiCameraHistoryDataList outputData;
    for (const auto& camera : request.resList)
    {
        QnChunksRequestData updatedRequest = request;
        updatedRequest.resList.clear();
        updatedRequest.resList.push_back(camera);
        MultiServerPeriodDataList chunks = QnMultiserverChunksRestHandler::loadDataSync(updatedRequest, owner);
        ec2::ApiCameraHistoryData outputRecord;
        outputRecord.cameraId = camera->getId();
        outputRecord.items = buildHistoryData(chunks);
        outputData.push_back(std::move(outputRecord));
    }

    QnFusionRestHandlerDetail::serialize(outputData, result, contentType, request.format);
    return nx_http::StatusCode::ok;
}
