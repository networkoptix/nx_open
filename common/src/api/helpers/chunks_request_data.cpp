#include "chunks_request_data.h"

#include <common/common_globals.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>


#include <utils/serialization/lexical.h>
#include <utils/common/util.h>
#include <utils/common/string.h>

namespace {
    const QString startTimeKey(lit("startTime"));
    const QString endTimeKey(lit("endTime"));
    const QString detailKey(lit("detail"));
    const QString periodsTypeKey(lit("periodsType"));
    const QString filterKey(lit("filter"));
    const QString localKey(lit("local"));
    const QString formatKey(lit("format"));
    const QString physicalIdKey(lit("physicalId"));
    const QString macKey(lit("mac"));
    const QString idKey(lit("id"));
    const QString limitKey(lit("limit"));
    const QString flatKey(lit("flat"));
}


QnChunksRequestData::QnChunksRequestData() :
    periodsType(Qn::RecordingContent), 
    startTimeMs(0), 
    endTimeMs(DATETIME_NOW), 
    detailLevel(1), 
    isLocal(false), 
    format(Qn::JsonFormat),
    limit(INT_MAX),
    flat(false)
{

}


QnChunksRequestData QnChunksRequestData::fromParams(const QnRequestParamList& params)
{
    static const qint64 USEC_PER_MS = 1000;

    QnChunksRequestData request;

    if (params.contains(startTimeKey))
        request.startTimeMs = parseDateTime(params.value(startTimeKey)) / USEC_PER_MS;

    if (params.contains(endTimeKey))
        request.endTimeMs = parseDateTime(params.value(endTimeKey)) / USEC_PER_MS;

    if (params.contains(detailKey))
        request.detailLevel = params.value(detailKey).toLongLong();

    if (params.contains(periodsTypeKey))
        request.periodsType = static_cast<Qn::TimePeriodContent>(params.value(periodsTypeKey).toInt());

    if (params.contains(limitKey))
        request.limit = qMax(0LL, params.value(limitKey).toLongLong());

    request.flat = params.contains(flatKey);

    request.filter = params.value(filterKey);
    request.isLocal = params.contains(localKey);
    QnLexical::deserialize(params.value(formatKey), &request.format);

    for (const auto& id: params.allValues(physicalIdKey)) {
        QnVirtualCameraResourcePtr camRes = qnResPool->getNetResourceByPhysicalId(id).dynamicCast<QnVirtualCameraResource>();
        if (camRes)
            request.resList << camRes;
    }
    for (const auto& id: params.allValues(macKey)) {
        QnVirtualCameraResourcePtr camRes = qnResPool->getResourceByMacAddress(id).dynamicCast<QnVirtualCameraResource>();
        if (camRes)
            request.resList << camRes;
    }
    for (const auto& id: params.allValues(idKey)) {
        QnVirtualCameraResourcePtr camRes = qnResPool->getResourceById(id).dynamicCast<QnVirtualCameraResource>();
        if (camRes)
            request.resList << camRes;
    }

    return request;
}

QnRequestParamList QnChunksRequestData::toParams() const
{
    QnRequestParamList result;

    result.insert(startTimeKey,     QString::number(startTimeMs));
    result.insert(endTimeKey,       QString::number(endTimeMs));
    result.insert(detailKey,        QString::number(detailLevel));
    result.insert(periodsTypeKey,   QString::number(periodsType));
    result.insert(filterKey,        filter);
    result.insert(limitKey,         QString::number(limit));
    if (isLocal)
        result.insert(localKey, QString());
    result.insert(formatKey, QnLexical::serialized(format));

    for (const auto &camera: resList)
        result.insert(physicalIdKey, camera->getPhysicalId());

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
    return !resList.isEmpty() 
        && endTimeMs > startTimeMs 
        && format != Qn::UnsupportedFormat;
}
