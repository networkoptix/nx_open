#include "bookmark_request_data.h"


#include <common/common_globals.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>


#include <utils/serialization/lexical.h>
#include <utils/common/util.h>


namespace {
    const QString startTimeKey(lit("startTime"));
    const QString endTimeKey(lit("endTime"));
    const QString strategyKey(lit("strategy"));
    const QString filterKey(lit("filter"));
    const QString localKey(lit("local"));
    const QString formatKey(lit("format"));
    const QString physicalIdKey(lit("physicalId"));
    const QString macKey(lit("mac"));
    const QString idKey(lit("id"));
    const QString limitKey(lit("limit"));
}

QnBookmarkRequestData::QnBookmarkRequestData():
    strategy(Qn::EarliestFirst), 
    format(Qn::JsonFormat),
    startTimeMs(0), 
    endTimeMs(DATETIME_NOW), 
    isLocal(false), 
    limit(std::numeric_limits<int>().max())
{

}

QnBookmarkRequestData QnBookmarkRequestData::fromParams(const QnRequestParamList& params) {
    QnBookmarkRequestData request;

    if (params.contains(startTimeKey))
        request.startTimeMs = parseDateTime(params.value(startTimeKey));

    if (params.contains(endTimeKey))
        request.endTimeMs = parseDateTime(params.value(endTimeKey));

    //if (params.contains(strategyKey())) {
        QnLexical::deserialize(params.value(strategyKey), &request.strategy);
        //request.strategy = QnLexical::deserialized(params.value(periodsTypeKey), Qn::EarliestFirst);
            //static_cast<Qn::BookmarkSearchStrategy>(.toInt());
    //}

    if (params.contains(limitKey))
        request.limit = qMax(0LL, params.value(limitKey).toLongLong());

    request.filter = params.value(filterKey);
    request.isLocal = params.contains(localKey);
    QnLexical::deserialize(params.value(formatKey), &request.format);

    for (const auto& id: params.allValues(physicalIdKey)) {
        if (QnVirtualCameraResourcePtr camera = qnResPool->getNetResourceByPhysicalId(id).dynamicCast<QnVirtualCameraResource>())
            request.cameras << camera;
    }
    for (const auto& id: params.allValues(macKey)) {
        if (QnVirtualCameraResourcePtr camera = qnResPool->getResourceByMacAddress(id).dynamicCast<QnVirtualCameraResource>())
            request.cameras << camera;
    }
    for (const auto& id: params.allValues(idKey)) {
        if (QnVirtualCameraResourcePtr camera = qnResPool->getResourceById(id).dynamicCast<QnVirtualCameraResource>())
            request.cameras << camera;
    }

    return request;
}

QnRequestParamList QnBookmarkRequestData::toParams() const {
    QnRequestParamList result;

    result.insert(startTimeKey,     QString::number(startTimeMs));
    result.insert(endTimeKey,       QString::number(endTimeMs));
    result.insert(filterKey,        filter);
    result.insert(limitKey,         QString::number(limit));
    if (isLocal)
        result.insert(localKey, QString());
    result.insert(formatKey,        QnLexical::serialized(format));
    result.insert(strategyKey,      QnLexical::serialized(strategy));

    for (const auto &camera: cameras)
        result.insert(physicalIdKey, camera->getPhysicalId());

    return result;
}

QUrlQuery QnBookmarkRequestData::toUrlQuery() const {
    QUrlQuery urlQuery;
    for(const auto& param: toParams())
        urlQuery.addQueryItem(param.first, param.second);
    return urlQuery;
}

bool QnBookmarkRequestData::isValid() const {
    return !cameras.isEmpty() 
        && endTimeMs > startTimeMs 
        && format != Qn::UnsupportedFormat;
}
