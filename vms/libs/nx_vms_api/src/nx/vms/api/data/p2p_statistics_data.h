#pragma once

#include "data.h"

#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API P2pStatisticsData: public Data
{
    qint64 totalBytesSent = 0;
    qint64 totalDbData = 0;
    QMap<QString, qint64> p2pCounters;
};

#define P2pStatisticsData_Fields \
    (totalBytesSent) \
    (totalDbData) \
    (p2pCounters)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::P2pStatisticsData)
