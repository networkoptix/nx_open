#include <QUrlQuery>
#include <QWaitCondition>

#include <vector>

#include <api/helpers/chunks_request_data.h>

#include "camera_history_rest_handler.h"
#include "core/resource/camera_resource.h"
#include <core/resource/camera_history.h>

namespace {

static const int kHistoryCacheTimeoutMs = 1000 * 60 * 10;

struct TimePeriodEx: public QnTimePeriod
{
    int index;

    TimePeriodEx(const QnTimePeriod& period, int index): QnTimePeriod(period), index(index) {}
    TimePeriodEx(): index(-1) {}
    operator bool() const { return !isNull(); }
};

TimePeriodEx findMinStartTimePeriod(
    const std::vector<int>& scanPos,
    const MultiServerPeriodDataList& chunks)
{
    TimePeriodEx result;
    for (int i = 0; i < scanPos.size(); ++i)
    {
        if (scanPos[i] >= chunks[i].periods.size())
            continue; //< no periods left

        const auto& period = chunks[i].periods[scanPos[i]];
        if (result.isNull() ||
            period.startTimeMs < result.startTimeMs ||
           (period.startTimeMs == result.startTimeMs && period.endTimeMs() > result.endTimeMs()))
        {
            result = TimePeriodEx(period, i);
        }
    }
    return result;
}

struct LoadDataContext
{
    LoadDataContext()
    {
        timer.invalidate();
    }

    QnMutex mutex;
    QElapsedTimer timer;
};

LoadDataContext* getContext(const QnUuid& id)
{
    static QnMutex access;
    QnMutexLocker lock(&access);

    static std::map<QnUuid, std::unique_ptr<LoadDataContext>> loadDataContext;

    auto itr = loadDataContext.find(id);
    if (itr == loadDataContext.end())
        itr = loadDataContext.emplace(id, std::unique_ptr<LoadDataContext>(new LoadDataContext)).first;

    return itr->second.get();
}

} // namespace

ec2::ApiCameraHistoryItemDataList QnCameraHistoryRestHandler::buildHistoryData(const MultiServerPeriodDataList& chunks)
{
    ec2::ApiCameraHistoryItemDataList result;
    std::vector<int> scanPos(chunks.size());
    TimePeriodEx prevPeriod;

    while (auto period = findMinStartTimePeriod(scanPos, chunks))
    {
        ++scanPos[period.index];
        if (prevPeriod.contains(period))
            continue; //< no time advance
        if (prevPeriod.index != period.index)
            result.push_back(ec2::ApiCameraHistoryItemData(chunks[period.index].guid, period.startTimeMs));
        prevPeriod = period;
    }
    return result;
}

int QnCameraHistoryRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    QnChunksRequestData request = QnChunksRequestData::fromParams(params);
    if (!request.isValid())
        return nx_http::StatusCode::badRequest;

    ec2::ApiCameraHistoryDataList outputData;
    for (const auto& camera: request.resList)
    {
        ec2::ApiCameraHistoryData outputRecord;
        outputRecord.cameraId = camera->getId();

        bool isValid = false;

        LoadDataContext* context = getContext(outputRecord.cameraId);
        QnMutexLocker lock(&context->mutex);

        if (context->timer.isValid() && !context->timer.hasExpired(kHistoryCacheTimeoutMs))
            outputRecord.items = qnCameraHistoryPool->getHistoryDetails(outputRecord.cameraId, &isValid);
        if (!isValid)
        {
            QnChunksRequestData updatedRequest = request;
            updatedRequest.resList.clear();
            updatedRequest.resList.push_back(camera);
            MultiServerPeriodDataList chunks = QnMultiserverChunksRestHandler::loadDataSync(updatedRequest, owner);
            outputRecord.items = buildHistoryData(chunks);
            if (qnCameraHistoryPool->testAndSetHistoryDetails(outputRecord.cameraId, outputRecord.items))
                context->timer.restart();
        }
        outputData.push_back(std::move(outputRecord));
    }

    QnFusionRestHandlerDetail::serialize(outputData, result, contentType, request.format);
    return nx_http::StatusCode::ok;
}
