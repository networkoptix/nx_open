#pragma once

#include <nx_ec/data/api_p2p_statistics_data.h>

class QnCommonModule;

namespace rest {
namespace helper {

class P2pStatistics
{
public:
    static ec2::ApiP2pStatisticsData data(QnCommonModule* commonModule);
    static QByteArray kUrlPath;
};

} // helper
} // rest
