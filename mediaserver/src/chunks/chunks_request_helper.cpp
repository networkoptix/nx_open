#include "chunks_request_helper.h"
#include "recorder/storage_manager.h"
#include "core/resource/camera_bookmark.h"
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include "motion/motion_helper.h"
#include <utils/serialization/json.h>
#include <utils/serialization/json_functions.h>
#include "core/resource_management/resource_pool.h"

QnChunksRequestData QnChunksRequestData::fromParams(const QnRequestParamList& params)
{
    QnChunksRequestData request;
    if (params.contains("startTime"))
        request.startTimeMs = params.value("startTime").toLongLong();
    if (params.contains("endTime"))
        request.endTimeMs = params.value("endTime").toLongLong();
    if (params.contains("detail"))
        request.detailLevel = params.value("detail").toLongLong();
    if (params.contains("periodsType"))
        request.periodsType = static_cast<Qn::TimePeriodContent>(params.value("periodsType").toInt());
    request.filter = params.value("filter");
    request.isLocal = params.contains("local");
    QnLexical::deserialize(params.value(lit("format")), &request.format);

    for (const auto& id: params.allValues("physicalId")) {
        QnVirtualCameraResourcePtr camRes = qnResPool->getNetResourceByPhysicalId(id).dynamicCast<QnVirtualCameraResource>();
        if (camRes)
            request.resList << camRes;
    }
    for (const auto& id: params.allValues("mac")) {
        QnVirtualCameraResourcePtr camRes = qnResPool->getResourceByMacAddress(id).dynamicCast<QnVirtualCameraResource>();
        if (camRes)
            request.resList << camRes;
    }
    for (const auto& id: params.allValues("id")) {
        QnVirtualCameraResourcePtr camRes = qnResPool->getResourceById(id).dynamicCast<QnVirtualCameraResource>();
        if (camRes)
            request.resList << camRes;
    }

    return request;
}

QnRequestParamList QnChunksRequestData::toParams() const
{
    QnRequestParamList result;

    result.insert("startTime", QString::number(startTimeMs));
    result.insert("endTime", QString::number(endTimeMs));
    result.insert("detail", QString::number(detailLevel));
    result.insert("periodsType", QString::number(periodsType));
    if (!filter.isEmpty())
        result.insert("filter", filter);
    if (isLocal)
        result.insert("local", QString());
    result.insert("format", QnLexical::serialized(format));

    return result;
}

QUrlQuery QnChunksRequestData::toUrlQuery() const
{
    QUrlQuery urlQuery;
    for(const auto& param: toParams())
        urlQuery.addQueryItem(param.first, param.second);
    return urlQuery;
}

bool QnChunksRequestData::isValid() const
{
    return !resList.isEmpty() && endTimeMs > startTimeMs;
}

QnTimePeriodList QnChunksRequestHelper::load(const QnChunksRequestData& request)
{
    QnTimePeriodList periods;
    switch (request.periodsType) {
    case Qn::MotionContent:
        {
            QList<QRegion> motionRegions = QJson::deserialized<QList<QRegion>>(request.filter.toUtf8());
            periods = QnMotionHelper::instance()->matchImage(motionRegions, request.resList, request.startTimeMs, request.endTimeMs, request.detailLevel);
        }
        break;
    case Qn::BookmarksContent:
        {
            QnCameraBookmarkTags tags;
            if (!request.filter.isEmpty()) {
                QnCameraBookmarkSearchFilter bookmarksFilter;
                bookmarksFilter.minStartTimeMs = request.startTimeMs;
                bookmarksFilter.maxStartTimeMs = request.endTimeMs;
                bookmarksFilter.minDurationMs = request.detailLevel;
                bookmarksFilter.text = request.filter;
                QnCameraBookmarkList bookmarks;
                
                std::vector<QnTimePeriodList> tmpData;
                for (const QnVirtualCameraResourcePtr& res: request.resList)
                {
                    QnTimePeriodList periods;
                    if (qnStorageMan->getBookmarks(res->getPhysicalId().toUtf8(), bookmarksFilter, bookmarks)) {
                        for (const QnCameraBookmark &bookmark: bookmarks)
                            periods.push_back(QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs));
                    }
                    tmpData.push_back(std::move(periods));
                }
                periods = QnTimePeriodList::mergeTimePeriods(tmpData);
            } else {
                //TODO: #GDM #Bookmarks use tags to filter periods?
                periods = qnStorageMan->getRecordedPeriods(request.resList, request.startTimeMs, request.endTimeMs, request.detailLevel, QList<QnServer::ChunksCatalog>() << QnServer::BookmarksCatalog);
            }
            break;
        }
    case Qn::RecordingContent:
    default:
        periods = qnStorageMan->getRecordedPeriods(request.resList, request.startTimeMs, request.endTimeMs, request.detailLevel, QList<QnServer::ChunksCatalog>() << QnServer::LowQualityCatalog << QnServer::HiQualityCatalog);
        break;
    }

    return periods;
}
