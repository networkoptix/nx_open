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
    CandidateTimePeriod(const QnUuid& serverId, const QnTimePeriod& period, int index)
        : serverId(serverId),
        period(period),
        index(index)
    {}
    CandidateTimePeriod() : index(-1) {}
    bool isNull() const { return period.isNull(); }
};

std::vector<CandidateTimePeriod> findMinStartTimePeriod(
    const std::vector<int>& scanPos,
    const MultiServerPeriodDataList& chunks)
{
    std::vector<CandidateTimePeriod> result;
    qint64 minTime = INT64_MAX;

    for (int i = 0; i < scanPos.size(); ++i)
    {
        if (scanPos[i] >= chunks[i].periods.size())
            continue; //< no periods left

        const auto& period = chunks[i].periods[scanPos[i]];
        if (period.startTimeMs <= minTime)
        {
            if (period.startTimeMs < minTime)
                result.clear();
            minTime = period.startTimeMs;
            result.push_back(CandidateTimePeriod(chunks[i].guid, period, i));
        }
    }
    return result;
}

} // namespace

ec2::ApiCameraHistoryItemDataList QnCameraHistoryRestHandler::buildHistoryData(const MultiServerPeriodDataList& chunks)
{
    ec2::ApiCameraHistoryItemDataList result;
    std::vector<int> scanPos;
    scanPos.resize(chunks.size());

    CandidateTimePeriod prevPeriod;
    while (true)
    {
        auto candidateTimePeriods = findMinStartTimePeriod(scanPos, chunks);
        if (candidateTimePeriods.empty())
            break;
        std::sort(candidateTimePeriods.begin(), candidateTimePeriods.end(), [](const CandidateTimePeriod& p1, const CandidateTimePeriod& p2)
        {
            return p1.period.durationMs > p2.period.durationMs;
        });
        const auto& candidate = candidateTimePeriods[0];

        bool candidateLastsLongerThenPrev = !prevPeriod.period.contains(candidate.period);
        bool candidateSuccessfull = prevPeriod.isNull() ||
            (prevPeriod.serverId != candidate.serverId && candidateLastsLongerThenPrev);

        if (prevPeriod.isNull() || candidateSuccessfull)
            result.push_back(ec2::ApiCameraHistoryItemData(candidate.serverId, candidate.period.startTimeMs));

        if (candidateLastsLongerThenPrev)
            prevPeriod = candidate;

        for (auto& p : candidateTimePeriods)
            ++scanPos[p.index];
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
