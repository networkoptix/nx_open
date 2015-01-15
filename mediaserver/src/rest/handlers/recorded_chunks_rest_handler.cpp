#include "recorded_chunks_rest_handler.h"

#include "recorder/storage_manager.h"
#include "utils/network/tcp_connection_priv.h"

#include "core/resource_management/resource_pool.h"
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>

#include "utils/common/util.h"
#include <utils/fs/file.h>
#include "motion/motion_helper.h"
#include "api/serializer/serializer.h"

int QnRecordedChunksRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path)
    qint64 startTime = -1, endTime = 1, detailLevel = -1;
    QnResourceList resList;
    QByteArray errStr;
    QByteArray errStrPhysicalId;
    bool urlFound = false;
    QString physicalId;
    
    ChunkFormat format = ChunkFormat_Unknown;
    QString callback;
    Qn::TimePeriodContent periodsType = Qn::TimePeriodContentCount;
    QString filter;

    for (int i = 0; i < params.size(); ++i)
    {
        if (params[i].first == "physicalId" || params[i].first == "mac") // use 'mac' name for compatibility with previous client version
        {
            urlFound = true;
            physicalId = params[i].second.trimmed();
            QnResourcePtr res = qnResPool->getNetResourceByPhysicalId(physicalId);
            if (res == 0)
                errStrPhysicalId += QByteArray("Resource with physicalId '") + physicalId.toUtf8() + QByteArray("' not found. \n");
            else 
                resList << res;
        }
        else if (params[i].first == "startTime") 
        {
            startTime = parseDateTime(params[i].second.toUtf8());
        }
        else if (params[i].first == "endTime") {
            endTime = parseDateTime(params[i].second.toUtf8());
        }
        else if (params[i].first == "detail")
            detailLevel = params[i].second.toLongLong();
        else if (params[i].first == "format") 
        {
            if (params[i].second == "bin")
                format = ChunkFormat_Binary;
            else if (params[i].second == "bii")
                format = ChunkFormat_BinaryIntersected;
            else if (params[i].second == "xml")
                format = ChunkFormat_XML;
            else if (params[i].second == "txt")
                format = ChunkFormat_Text;
            else
                format = ChunkFormat_Json;
        }
        else if (params[i].first == "callback")
            callback = params[i].second;
        else if (params[i].first == "filter")
            filter = params[i].second;
        else if (params[i].first == "periodsType")
            periodsType = static_cast<Qn::TimePeriodContent>(params[i].second.toInt());
    }
    if (!urlFound)
        errStr += "Parameter physicalId must be provided. \n";
    if (startTime < 0)
        errStr += "Parameter startTime must be provided. \n";
    if (endTime < 0)
        errStr += "Parameter endTime must be provided. \n";
    if (detailLevel < 0)
        errStr += "Parameter detail must be provided. \n";

    if (resList.isEmpty())
        errStr += errStrPhysicalId;

    auto errLog = [&](const QString &errText) {
        result.append("<root>\n");
        result.append(errText);
        result.append("</root>\n");
        return CODE_INVALID_PARAMETER;
    };

    if (!errStr.isEmpty())
        return errLog(errStr);
    
    QnTimePeriodList periods;
    switch (periodsType) {
    case Qn::RecordingContent:
        periods = qnStorageMan->getRecordedPeriods(resList.filtered<QnVirtualCameraResource>(), startTime, endTime, detailLevel, QList<QnServer::ChunksCatalog>() << QnServer::LowQualityCatalog << QnServer::HiQualityCatalog);
        break;
    case Qn::MotionContent:
        {
            QList<QRegion> motionRegions;
            parseRegionList(motionRegions, filter);
            periods = QnMotionHelper::instance()->matchImage(motionRegions, resList, startTime, endTime, detailLevel);
        }
        break;
#ifdef QN_ENABLE_BOOKMARKS
    case Qn::BookmarksContent:
        {
            QnCameraBookmarkTags tags;
            if (!filter.isEmpty()) {
                QnCameraBookmarkSearchFilter bookmarksFilter;
                bookmarksFilter.minStartTimeMs = startTime;
                bookmarksFilter.maxStartTimeMs = endTime;
                bookmarksFilter.minDurationMs = detailLevel;
                bookmarksFilter.text = filter;
                QnCameraBookmarkList bookmarks;
                if (qnStorageMan->getBookmarks(physicalId.toUtf8(), bookmarksFilter, bookmarks)) {
                    for (const QnCameraBookmark &bookmark: bookmarks)
                        periods << QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs);                    
                }
            } else {
            //TODO: #GDM #Bookmarks use tags to filter periods?
                periods = qnStorageMan->getRecordedPeriods(resList.filtered<QnVirtualCameraResource>(), startTime, endTime, detailLevel, QList<QnServer::ChunksCatalog>() << QnServer::BookmarksCatalog);
            }
            break;
        }
#endif
    default:
        return errLog("Invalid periodsType parameter.");
    }

    
    switch(format) 
    {
        case ChunkFormat_Binary:
            result.append("BIN");
            periods.encode(result, false);
            break;
        case ChunkFormat_BinaryIntersected:
            result.append("BII");
            periods.encode(result, true);
            break;
        case ChunkFormat_XML:
            result.append("<recordedTimePeriods xmlns=\"http://www.networkoptix.com/xsd/api/recordedTimePeriods\">\n");
            for(const QnTimePeriod& period: periods)
                result.append(QString("<timePeriod startTime=\"%1\" duration=\"%2\" />\n").arg(period.startTimeMs).arg(period.durationMs));
            result.append("</recordedTimePeriods>\n");
            break;
        case ChunkFormat_Text:
            result.append("<root>\n");
            for(const QnTimePeriod& period: periods) {
                result.append("<chunk>");
                result.append(QDateTime::fromMSecsSinceEpoch(period.startTimeMs).toString(QLatin1String("dd-MM-yyyy hh:mm:ss.zzz")));
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
                    .append(QByteArray::number(period.startTimeMs)).append(", ").append(QByteArray::number(period.durationMs))
                    .append("]");
            }
                
            result.append("]");
            break;
    }

    return CODE_OK;
}

int QnRecordedChunksRestHandler::executePost(const QString& path, const QnRequestParamList& params, const QByteArray& /*body*/, const QByteArray& /*srcBodyContentType*/, QByteArray& result, 
                                             QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    return executeGet(path, params, result, contentType, owner);
}

