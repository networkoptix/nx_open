#pragma once

#include <nx/vms/api/data/statistics_data.h>

namespace nx::vms::server::statistics {

nx::vms::api::StatisticsCameraData toStatisticsData(const nx::vms::api::CameraDataEx& data);

nx::vms::api::StatisticsStorageData toStatisticsData(const nx::vms::api::StorageData& data);

nx::vms::api::StatisticsMediaServerData toStatisticsData(
    const nx::vms::api::MediaServerDataEx& data);

nx::vms::api::StatisticsLicenseData toStatisticsData(const nx::vms::api::LicenseData& data);

nx::vms::api::StatisticsEventRuleData toStatisticsData(const nx::vms::api::EventRuleData& data);

nx::vms::api::StatisticsUserData toStatisticsData(const nx::vms::api::UserData& data);

nx::vms::api::StatisticsPluginInfo toStatisticsData(const nx::vms::api::ExtendedPluginInfo& data);

} // namespace nx::vms::server::statistics
