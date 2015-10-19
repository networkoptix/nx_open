#include "bookmark_request_data.h"

#include <common/common_globals.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/model_functions.h>
#include <utils/common/util.h>
#include <utils/common/string.h>


namespace {
    const QString startTimeKey      (lit("startTime"));
    const QString endTimeKey        (lit("endTime"));
    const QString strategyKey       (lit("strategy"));
    const QString filterKey         (lit("filter"));
    const QString formatKey         (lit("format"));
    const QString physicalIdKey     (lit("physicalId"));
    const QString macKey            (lit("mac"));
    const QString cameraIdKey       (lit("cameraId"));
    const QString limitKey          (lit("limit"));
    const QString guidKey           (lit("guid"));
    const QString nameKey           (lit("name"));
    const QString descriptionKey    (lit("description"));
    const QString timeoutKey        (lit("timeout"));
    const QString durationKey       (lit("duration"));
    const QString tagKey            (lit("tag"));

    const QChar tagsDelimiter = L',';
    const qint64 USEC_PER_MS = 1000;
}

/************************************************************************/
/* QnGetBookmarksRequestData                                          */
/************************************************************************/

QnGetBookmarksRequestData::QnGetBookmarksRequestData()
    : QnMultiserverRequestData()
    , filter()
#ifdef _DEBUG
    , format(Qn::JsonFormat)
#else
    , format(Qn::UbjsonFormat)
#endif
    , cameras()
{

}

void QnGetBookmarksRequestData::loadFromParams(const QnRequestParamList& params) {
    QnMultiserverRequestData::loadFromParams(params);

    if (params.contains(startTimeKey))
        filter.startTimeMs = parseDateTime(params.value(startTimeKey)) / USEC_PER_MS;

    if (params.contains(endTimeKey))
        filter.endTimeMs = parseDateTime(params.value(endTimeKey))  / USEC_PER_MS;

    QnLexical::deserialize(params.value(strategyKey), &filter.strategy);

    if (params.contains(limitKey))
        filter.limit = qMax(0LL, params.value(limitKey).toLongLong());

    filter.text = params.value(filterKey);
    QnLexical::deserialize(params.value(formatKey), &format);

    for (const auto& id: params.allValues(physicalIdKey)) {
        if (QnVirtualCameraResourcePtr camera = qnResPool->getNetResourceByPhysicalId(id).dynamicCast<QnVirtualCameraResource>())
            cameras << camera;
    }
    for (const auto& id: params.allValues(macKey)) {
        if (QnVirtualCameraResourcePtr camera = qnResPool->getResourceByMacAddress(id).dynamicCast<QnVirtualCameraResource>())
            cameras << camera;
    }
    for (const auto& id: params.allValues(cameraIdKey)) {
        if (QnVirtualCameraResourcePtr camera = qnResPool->getResourceById(id).dynamicCast<QnVirtualCameraResource>())
            cameras << camera;
    }
}


QnRequestParamList QnGetBookmarksRequestData::toParams() const {
    QnRequestParamList result = QnMultiserverRequestData::toParams();

    result.insert(startTimeKey,     QnLexical::serialized(filter.startTimeMs));
    result.insert(endTimeKey,       QnLexical::serialized(filter.endTimeMs));
    result.insert(filterKey,        QnLexical::serialized(filter.text));
    result.insert(limitKey,         QnLexical::serialized(filter.limit));
    result.insert(strategyKey,      QnLexical::serialized(filter.strategy));
    result.insert(formatKey,        QnLexical::serialized(format));

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
    , format(Qn::JsonFormat)
{}

int QnGetBookmarkTagsRequestData::unlimited() {
    return std::numeric_limits<int>().max();
}

void QnGetBookmarkTagsRequestData::loadFromParams(const QnRequestParamList& params) {
    QnMultiserverRequestData::loadFromParams(params);
    limit = QnLexical::deserialized<int>(params.value(limitKey), unlimited());
    QnLexical::deserialize(params.value(formatKey), &format);
}

QnRequestParamList QnGetBookmarkTagsRequestData::toParams() const {
    QnRequestParamList result = QnMultiserverRequestData::toParams();
    result.insert(limitKey,     QnLexical::serialized(limit));
    result.insert(formatKey,    QnLexical::serialized(format));
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
