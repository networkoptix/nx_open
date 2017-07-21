#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2 {

struct ApiP2pStatisticsData
{
    qint64 totalBytesSent = 0;
    qint64 totalDbData = 0;
    QMap<QString, qint64> p2pCounters;
};

#define ApiP2pStatisticsData_Fields \
    (totalBytesSent) \
    (totalDbData) \
    (p2pCounters)

//-------------------------------------------------------------------------------------------------

} // namespace ec2
