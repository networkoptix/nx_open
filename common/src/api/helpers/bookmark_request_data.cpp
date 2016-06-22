#include "bookmark_request_data.h"

#include <common/common_globals.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/fusion/model_functions.h>
#include <utils/common/util.h>
#include <utils/common/string.h>


namespace
{
    const QString startTimeKey                  (lit("startTime"));
    const QString endTimeKey                    (lit("endTime"));
    const QString sortColumnKey                 (lit("sortBy"));
    const QString sortOrderKey                  (lit("sortOrder"));
    const QString useSparsing                   (lit("sparse"));
    const QString minVisibleLengthMs            (lit("minVisibleLength"));
    const QString filterKey                     (lit("filter"));
    const QString physicalIdKey                 (lit("physicalId"));
    const QString macKey                        (lit("mac"));
    const QString cameraIdKey                   (lit("cameraId"));
    const QString limitKey                      (lit("limit"));
    const QString guidKey                       (lit("guid"));
    const QString nameKey                       (lit("name"));
    const QString descriptionKey                (lit("description"));
    const QString timeoutKey                    (lit("timeout"));
    const QString durationKey                   (lit("duration"));
    const QString tagKey                        (lit("tag"));

    const QChar tagsDelimiter = L',';
    const qint64 USEC_PER_MS = 1000;
}

/************************************************************************/
/* QnGetBookmarksRequestData                                          */
/************************************************************************/

QnGetBookmarksRequestData::QnGetBookmarksRequestData()
    : QnMultiserverRequestData()
    , filter()
    , cameras()
{}

void QnGetBookmarksRequestData::loadFromParams(const QnRequestParamList& params) {
    QnMultiserverRequestData::loadFromParams(params);

    if (params.contains(startTimeKey))
        filter.startTimeMs = parseDateTime(params.value(startTimeKey)) / USEC_PER_MS;

    if (params.contains(endTimeKey))
        filter.endTimeMs = parseDateTime(params.value(endTimeKey))  / USEC_PER_MS;

    QnLexical::deserialize(params.value(sortColumnKey), &filter.orderBy.column);
    QnLexical::deserialize(params.value(sortOrderKey), &filter.orderBy.order);

    QnLexical::deserialize(params.value(useSparsing), &filter.sparsing.used);
    QnLexical::deserialize(params.value(minVisibleLengthMs), &filter.sparsing.minVisibleLengthMs);

    if (params.contains(limitKey))
        filter.limit = qMax(0LL, params.value(limitKey).toLongLong());

    filter.text = params.value(filterKey);

    for (const auto& id: params.allValues(physicalIdKey)) {
        if (QnVirtualCameraResourcePtr camera = qnResPool->getNetResourceByPhysicalId(id).dynamicCast<QnVirtualCameraResource>())
            cameras << camera;
    }
    for (const auto& id: params.allValues(macKey)) {
        if (QnVirtualCameraResourcePtr camera = qnResPool->getResourceByMacAddress(id).dynamicCast<QnVirtualCameraResource>())
            cameras << camera;
    }
    for (const auto& id: params.allValues(cameraIdKey)) {
        QnUuid uuid = QnUuid::fromStringSafe(id);
        if (uuid.isNull())
            continue;
        if (QnVirtualCameraResourcePtr camera = qnResPool->getResourceById(uuid).dynamicCast<QnVirtualCameraResource>())
            cameras << camera;
    }
}


QnRequestParamList QnGetBookmarksRequestData::toParams() const {
    QnRequestParamList result = QnMultiserverRequestData::toParams();

    result.insert(startTimeKey,     QnLexical::serialized(filter.startTimeMs));
    result.insert(endTimeKey,       QnLexical::serialized(filter.endTimeMs));
    result.insert(filterKey,        QnLexical::serialized(filter.text));
    result.insert(limitKey,         QnLexical::serialized(filter.limit));

    result.insert(sortColumnKey,    QnLexical::serialized(filter.orderBy.column));
    result.insert(sortOrderKey,     QnLexical::serialized(filter.orderBy.order));

    result.insert(useSparsing,          QnLexical::serialized(filter.sparsing.used));
    result.insert(minVisibleLengthMs,   QnLexical::serialized(filter.sparsing.minVisibleLengthMs));

    for (const auto &camera: cameras)
        result.insert(physicalIdKey,QnLexical::serialized(camera->getPhysicalId()));

    return result;
}

bool QnGetBookmarksRequestData::isValid() const {
    return !cameras.isEmpty()
        && filter.endTimeMs > filter.startTimeMs
        && format != Qn::UnsupportedFormat;
}

/************************************************************************/
/* QnGetBookmarkTagsRequestData                                         */
/************************************************************************/

QnGetBookmarkTagsRequestData::QnGetBookmarkTagsRequestData(int limit)
    : QnMultiserverRequestData()
    , limit(limit)
{}

int QnGetBookmarkTagsRequestData::unlimited() {
    return std::numeric_limits<int>().max();
}

void QnGetBookmarkTagsRequestData::loadFromParams(const QnRequestParamList& params) {
    QnMultiserverRequestData::loadFromParams(params);
    limit = QnLexical::deserialized<int>(params.value(limitKey), unlimited());
}

QnRequestParamList QnGetBookmarkTagsRequestData::toParams() const {
    QnRequestParamList result = QnMultiserverRequestData::toParams();
    result.insert(limitKey,     QnLexical::serialized(limit));
    return result;
}

bool QnGetBookmarkTagsRequestData::isValid() const {
    return limit >= 0;
}


/************************************************************************/
/* QnUpdateBookmarkRequestData                                          */
/************************************************************************/
QnUpdateBookmarkRequestData::QnUpdateBookmarkRequestData()
    : QnMultiserverRequestData()
    , bookmark()
{}

QnUpdateBookmarkRequestData::QnUpdateBookmarkRequestData(const QnCameraBookmark &bookmark)
    : QnMultiserverRequestData()
    , bookmark(bookmark)
{}

void QnUpdateBookmarkRequestData::loadFromParams(const QnRequestParamList& params) {
    QnMultiserverRequestData::loadFromParams(params);
    bookmark.guid           = QnLexical::deserialized<QnUuid>(params.value(guidKey));
    bookmark.name           = params.value(nameKey);
    bookmark.description    = params.value(descriptionKey);
    bookmark.timeout        = QnLexical::deserialized<qint64>(params.value(timeoutKey));
    bookmark.startTimeMs    = QnLexical::deserialized<qint64>(params.value(startTimeKey));
    bookmark.durationMs     = QnLexical::deserialized<qint64>(params.value(durationKey));
    bookmark.cameraId       = params.value(cameraIdKey);
    bookmark.tags           = params.allValues(tagKey).toSet();
}

QnRequestParamList QnUpdateBookmarkRequestData::toParams() const {
    QnRequestParamList result = QnMultiserverRequestData::toParams();

    result.insert(guidKey,          QnLexical::serialized(bookmark.guid));
    result.insert(nameKey,          bookmark.name);
    result.insert(descriptionKey,   bookmark.description);
    result.insert(timeoutKey,       QnLexical::serialized(bookmark.timeout));
    result.insert(startTimeKey,     QnLexical::serialized(bookmark.startTimeMs));
    result.insert(durationKey,      QnLexical::serialized(bookmark.durationMs));
    result.insert(cameraIdKey,      bookmark.cameraId);
    for (const QString &tag: bookmark.tags)
        result.insert(tagKey, tag);

    return result;
}

bool QnUpdateBookmarkRequestData::isValid() const {
    return bookmark.isValid();
}


/************************************************************************/
/* QnDeleteBookmarkRequestData                                          */
/************************************************************************/
QnDeleteBookmarkRequestData::QnDeleteBookmarkRequestData()
    : QnMultiserverRequestData()
    , bookmarkId()
{}

QnDeleteBookmarkRequestData::QnDeleteBookmarkRequestData(const QnUuid &bookmarkId)
    : QnMultiserverRequestData()
    , bookmarkId(bookmarkId)
{}

void QnDeleteBookmarkRequestData::loadFromParams(const QnRequestParamList& params) {
    QnMultiserverRequestData::loadFromParams(params);
    bookmarkId = QnLexical::deserialized<QnUuid>(params.value(guidKey));
}

QnRequestParamList QnDeleteBookmarkRequestData::toParams() const  {
    QnRequestParamList result = QnMultiserverRequestData::toParams();
    result.insert(guidKey, QnLexical::serialized(bookmarkId));
    return result;
}

bool QnDeleteBookmarkRequestData::isValid() const {
    return !bookmarkId.isNull();
}
