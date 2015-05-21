#include "chunks_request_data.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/serialization/lexical.h>

QnChunksRequestData::QnChunksRequestData() :
    periodsType(Qn::RecordingContent), 
    startTimeMs(0), 
    endTimeMs(DATETIME_NOW), 
    detailLevel(1), 
    isLocal(false), 
    format(Qn::JsonFormat)
{

}


QnChunksRequestData QnChunksRequestData::fromParams(const QnRequestParamList& params)
{
    QnChunksRequestData request;
    if (params.contains(lit("startTime")))
        request.startTimeMs = params.value(lit("startTime")).toLongLong();
    if (params.contains(lit("endTime")))
        request.endTimeMs = params.value(lit("endTime")).toLongLong();
    if (params.contains(lit("detail")))
        request.detailLevel = params.value(lit("detail")).toLongLong();
    if (params.contains(lit("periodsType")))
        request.periodsType = static_cast<Qn::TimePeriodContent>(params.value(lit("periodsType")).toInt());
    request.filter = params.value(lit("filter"));
    request.isLocal = params.contains(lit("local"));
    QnLexical::deserialize(params.value(lit("format")), &request.format);

    for (const auto& id: params.allValues(lit("physicalId"))) {
        QnVirtualCameraResourcePtr camRes = qnResPool->getNetResourceByPhysicalId(id).dynamicCast<QnVirtualCameraResource>();
        if (camRes)
            request.resList << camRes;
    }
    for (const auto& id: params.allValues(lit("mac"))) {
        QnVirtualCameraResourcePtr camRes = qnResPool->getResourceByMacAddress(id).dynamicCast<QnVirtualCameraResource>();
        if (camRes)
            request.resList << camRes;
    }
    for (const auto& id: params.allValues(lit("id"))) {
        QnVirtualCameraResourcePtr camRes = qnResPool->getResourceById(id).dynamicCast<QnVirtualCameraResource>();
        if (camRes)
            request.resList << camRes;
    }

    return request;
}

QnRequestParamList QnChunksRequestData::toParams() const
{
    QnRequestParamList result;

    result.insert(lit("startTime"), QString::number(startTimeMs));
    result.insert(lit("endTime"), QString::number(endTimeMs));
    result.insert(lit("detail"), QString::number(detailLevel));
    result.insert(lit("periodsType"), QString::number(periodsType));
    if (!filter.isEmpty())
        result.insert(lit("filter"), filter);
    if (isLocal)
        result.insert(lit("local"), QString());
    result.insert(lit("format"), QnLexical::serialized(format));

    for (const auto &camera: resList)
        result.insert(lit("physicalId"), camera->getPhysicalId());

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
