#pragma once

#include <chrono>

#include <boost/optional.hpp>

#include <QUrlQuery>

#include <analytics/detected_objects_storage/analytics_events_storage_types.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>

struct QnChunksRequestData
{
    // Versions here are server versions.
    enum class RequestVersion
    {
        v2_6,
        v3_0,

        current = v3_0
    };

    QnChunksRequestData();

    static QnChunksRequestData fromParams(QnResourcePool* resourcePool, const QnRequestParamList& params);
    QnRequestParamList toParams() const;
    QUrlQuery toUrlQuery() const;
    bool isValid() const;

    RequestVersion requestVersion = RequestVersion::current;

    Qn::TimePeriodContent periodsType;
    QnSecurityCamResourceList resList;
    qint64 startTimeMs;
    qint64 endTimeMs;
    std::chrono::milliseconds detailLevel;
    bool keepSmallChunks;
    QString filter; //< TODO: This string is a json. Consider changing to QList<QRegion>.
    bool isLocal;
    Qn::SerializationFormat format;
    int limit;
    bool flat;

    boost::optional<nx::analytics::storage::Filter> analyticsStorageFilter;
};
