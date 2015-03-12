#ifndef __CHUNKS_REQUEST_DATA_H__
#define __CHUNKS_REQUEST_DATA_H__

#include <QUrlQuery>
#include "core/resource/resource_fwd.h"
#include "recording/time_period_list.h"
#include "utils/common/request_param.h"

struct QnChunksRequestData
{
    QnChunksRequestData(): periodsType(Qn::RecordingContent), startTimeMs(0), endTimeMs(DATETIME_NOW), detailLevel(1), isLocal(true), format(Qn::JsonFormat) {}

    static QnChunksRequestData fromParams(const QnRequestParamList& params);
    QnRequestParamList toParams() const;
    QUrlQuery toUrlQuery() const;

    Qn::TimePeriodContent periodsType;
    QnVirtualCameraResourceList resList;
    qint64 startTimeMs;
    qint64 endTimeMs;
    qint64 detailLevel;
    QString filter;
    bool isLocal;
    Qn::SerializationFormat format;
};

class QnChunksRequestHelper
{
public:
    static QnTimePeriodList load(const QnChunksRequestData& request);
};

#endif // __CHUNKS_REQUEST_DATA_H__
