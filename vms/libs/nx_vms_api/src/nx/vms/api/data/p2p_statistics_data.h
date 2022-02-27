// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API P2pStatisticsData
{
    qint64 totalBytesSent = 0;
    qint64 totalDbData = 0;
    QMap<QString, qint64> p2pCounters;
};
#define P2pStatisticsData_Fields \
    (totalBytesSent)(totalDbData)(p2pCounters)
NX_VMS_API_DECLARE_STRUCT(P2pStatisticsData)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::P2pStatisticsData)
