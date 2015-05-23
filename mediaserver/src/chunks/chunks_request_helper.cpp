#include "chunks_request_helper.h"
#include "recorder/storage_manager.h"
#include "core/resource/camera_bookmark.h"
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include "motion/motion_helper.h"
#include <utils/serialization/json.h>
#include <utils/serialization/json_functions.h>
#include "core/resource_management/resource_pool.h"

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
