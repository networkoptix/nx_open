#pragma once

#include <nx/vms/api/data/p2p_statistics_data.h>

class QnCommonModule;

namespace rest {
namespace helper {

class P2pStatistics
{
public:
    static nx::vms::api::P2pStatisticsData data(QnCommonModule* commonModule);
    static QByteArray kUrlPath;
};

} // namespace helper
} // namespace rest
