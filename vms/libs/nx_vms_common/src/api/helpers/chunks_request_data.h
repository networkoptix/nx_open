// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QUrlQuery>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/network/rest/params.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/datetime.h>
#include <nx/utils/serialization/format.h>
#include <nx/vms/api/types/storage_location.h>

class QnCommonModule;

struct NX_VMS_COMMON_API QnChunksRequestData
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(GroupBy,
        none,
        cameraId,
        serverId
    )

    QnChunksRequestData() = default;

    static QnChunksRequestData fromParams(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params);
    nx::network::rest::Params toParams() const;
    QUrlQuery toUrlQuery() const;
    bool isValid() const;

    Qn::TimePeriodContent periodsType = Qn::RecordingContent;
    QnVirtualCameraResourceList resList;
    qint64 startTimeMs = 0;
    qint64 endTimeMs = DATETIME_NOW;
    std::chrono::milliseconds detailLevel{1};
    bool keepSmallChunks = false;
    bool preciseBounds = false;

    // Serialized search filter. For motion requests it's a QList<QRegion> object, for analytics
    // it's an nx::analytics::db::Filter object.
    // TODO: unify filter object for all type of requests.
    QString filter;
    bool isLocal = false;
    Qn::SerializationFormat format = Qn::SerializationFormat::json;
    int limit = INT_MAX;

    GroupBy groupBy = GroupBy::serverId;
    Qt::SortOrder sortOrder = Qt::SortOrder::AscendingOrder;
    bool includeCloudData = false;
    nx::vms::api::StorageLocation storageLocation = nx::vms::api::StorageLocation::both;
};
