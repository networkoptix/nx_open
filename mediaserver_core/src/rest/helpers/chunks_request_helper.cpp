#include "chunks_request_helper.h"

#include <api/helpers/chunks_request_data.h>

#include "recorder/storage_manager.h"
#include "core/resource/camera_bookmark.h"
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include "motion/motion_helper.h"
#include <utils/serialization/json.h>
#include <utils/serialization/json_functions.h>
#include "core/resource_management/resource_pool.h"

#include <vector>

QnTimePeriodList QnChunksRequestHelper::load(const QnChunksRequestData& request)
{
    // TODO: #akulikov #backup storages: Alter this for two storage managers kinds.
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
                bookmarksFilter.startTimeMs = request.startTimeMs;
                bookmarksFilter.endTimeMs = request.endTimeMs;
                //bookmarksFilter.minDurationMs = request.detailLevel;
                bookmarksFilter.text = request.filter;
                QnCameraBookmarkList bookmarks;
                
                for (const QnVirtualCameraResourcePtr& res: request.resList) {
                    if (qnNormalStorageMan->getBookmarks(res->getPhysicalId().toUtf8(), bookmarksFilter, bookmarks)) {
                        for (const QnCameraBookmark &bookmark: bookmarks)
                            periods.includeTimePeriod(QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs));
                    }
                }
                
                for (const QnVirtualCameraResourcePtr& res: request.resList) {
                    if (qnBackupStorageMan->getBookmarks(res->getPhysicalId().toUtf8(), bookmarksFilter, bookmarks)) {
                        for (const QnCameraBookmark &bookmark: bookmarks)
                            periods.includeTimePeriod(QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs));
                    }
                }
            } else {
                //TODO: #GDM #Bookmarks use tags to filter periods?
                QnStorageManager::getRecordedPeriods(
                    request.resList, 
                    request.startTimeMs, 
                    request.endTimeMs, 
                    request.detailLevel, 
                    request.keepSmallChunks,
                    QList<QnServer::ChunksCatalog>() 
                        << QnServer::BookmarksCatalog, request.limit
                ).simplified();
            }
            break;
        }
    case Qn::RecordingContent:
    default:
        periods = QnStorageManager::getRecordedPeriods(request.resList, request.startTimeMs, request.endTimeMs, request.detailLevel, request.keepSmallChunks,
            QList<QnServer::ChunksCatalog>() << QnServer::LowQualityCatalog << QnServer::HiQualityCatalog, request.limit);
        break;
    }

    return periods;
}
