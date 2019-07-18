#pragma once

#include <chrono>

#include <boost/optional.hpp>

#include <QUrlQuery>

#include <analytics/db/analytics_db_types.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <common/common_globals.h>
#include <nx/utils/datetime.h>
#include <nx/fusion/model_functions_fwd.h>

struct QnChunksRequestData
{
    enum class GroupBy
    {
        none,
        cameraId,
        serverId
    };

    // Versions here are server versions.
    enum class RequestVersion
    {
        v2_6,
        v3_0,

        current = v3_0
    };

    QnChunksRequestData() = default;

    static QnChunksRequestData fromParams(QnResourcePool* resourcePool, const QnRequestParamList& params);
    QnRequestParamList toParams() const;
    QUrlQuery toUrlQuery() const;
    bool isValid() const;

    RequestVersion requestVersion = RequestVersion::current;

    Qn::TimePeriodContent periodsType = Qn::RecordingContent;
    QnVirtualCameraResourceList resList;
    qint64 startTimeMs = 0;
    qint64 endTimeMs = DATETIME_NOW;
    std::chrono::milliseconds detailLevel{1};
    bool keepSmallChunks = false;

    // Serialized search filter. For motion requests it's a QList<QRegion> object, for analytics
    // it's an nx::analytics::db::Filter object.
    // TODO: unify filter object for all type of requests.
    QString filter;
    bool isLocal = false;
    Qn::SerializationFormat format = Qn::JsonFormat;
    int limit = INT_MAX;

    GroupBy groupBy = GroupBy::serverId;
    Qt::SortOrder sortOrder = Qt::SortOrder::AscendingOrder;
};

QN_FUSION_DECLARE_FUNCTIONS(QnChunksRequestData::GroupBy, (lexical))
