#pragma once

#include <nx/vms/api/data/statistics_data.h>

namespace nx::vms::server::statistics {

nx::vms::api::StatisticsCameraData toStatisticsData(nx::vms::api::CameraDataEx&& data);

nx::vms::api::StatisticsStorageData toStatisticsData(nx::vms::api::StorageData&& data);

nx::vms::api::StatisticsMediaServerData toStatisticsData(nx::vms::api::MediaServerDataEx&& data);

nx::vms::api::StatisticsLicenseData toStatisticsData(const nx::vms::api::LicenseData& data);

nx::vms::api::StatisticsEventRuleData toStatisticsData(nx::vms::api::EventRuleData&& data);

nx::vms::api::StatisticsUserData toStatisticsData(nx::vms::api::UserData&& data);

nx::vms::api::StatisticsPluginInfo toStatisticsData(nx::vms::api::PluginInfoEx&& data);

} // namespace nx::vms::server::statistics
