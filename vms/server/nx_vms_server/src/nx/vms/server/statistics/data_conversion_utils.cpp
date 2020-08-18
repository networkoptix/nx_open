#include "data_conversion_utils.h"

#include <core/resource/camera_resource.h>
#include <core/resource/param.h>
#include <core/resource/resource_type.h>
#include <nx/analytics/properties.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

namespace nx::vms::server::statistics {

using namespace nx::vms::api;

StatisticsCameraData toStatisticsData(CameraDataEx&& data)
{
    StatisticsCameraData statisticsCameraData;
    (CameraDataEx&) statisticsCameraData = std::move(data);

    // find out if default password worked
    const auto& defCred = ResourcePropertyKey::kDefaultCredentials;
    const auto it = std::find_if(
        statisticsCameraData.addParams.begin(),
        statisticsCameraData.addParams.end(),
        [&defCred](const auto& param) { return param.name == defCred; });

    const bool isDefCred =
        (it != statisticsCameraData.addParams.end()) && !it->value.isEmpty();

    const static std::set<QString> kCameraParamsToRemove =
    {
        ResourcePropertyKey::kCredentials,
        ResourcePropertyKey::kDefaultCredentials,
        ResourcePropertyKey::kOnvifIgnoreMedia2,
        ResourcePropertyKey::Onvif::kDeviceID,
        ResourcePropertyKey::Onvif::kDeviceUrl,
        ResourcePropertyKey::Onvif::kMediaUrl,
        QnVirtualCameraResource::kUserEnabledAnalyticsEnginesProperty,
        QnVirtualCameraResource::kCompatibleAnalyticsEnginesProperty,
        QnVirtualCameraResource::kDeviceAgentsSettingsValuesProperty,
        QnVirtualCameraResource::kDeviceAgentManifestsProperty,
    };

    nx::utils::remove_if(
        statisticsCameraData.addParams,
        [](const auto& param) { return kCameraParamsToRemove.count(param.name); });

    statisticsCameraData.addParams.push_back(
        nx::vms::api::ResourceParamData(defCred, isDefCred ? lit("true") : lit("false")));

    const static std::vector<QString> kCameraParamsToDefault =
    {
        ResourcePropertyKey::kAnalog,
        ResourcePropertyKey::kCameraCapabilities,
        ResourcePropertyKey::kHasDualStreaming,
        ResourcePropertyKey::kIsAudioSupported,
        ResourcePropertyKey::kMediaCapabilities,
        ResourcePropertyKey::kPtzCapabilities,
        ResourcePropertyKey::kStreamFpsSharing,
        ResourcePropertyKey::kSupportedMotion,
    };

    for (const auto& param: kCameraParamsToDefault)
    {
        if (!qnResTypePool)
            return statisticsCameraData;

        const auto it = std::find_if(
            statisticsCameraData.addParams.begin(),
            statisticsCameraData.addParams.end(),
            [&param](const auto& d) { return d.name == param; });

        if (it != statisticsCameraData.addParams.end()
            && !it->value.isEmpty()
            && it->value != lit("0"))
        {
            continue;
        }

        if (const auto type = qnResTypePool->getResourceType(statisticsCameraData.typeId))
        {
            const auto value = type->defaultValue(param);
            if (!value.isEmpty())
            {
                statisticsCameraData.addParams.push_back(
                    nx::vms::api::ResourceParamData(param, value));
            }
        }
    }

    return statisticsCameraData;
}

StatisticsStorageData toStatisticsData(StorageData&& data)
{
    StatisticsStorageData statisticsStorageData;
    (StorageData&) statisticsStorageData = std::move(data);

    return statisticsStorageData;
}

StatisticsMediaServerData toStatisticsData(MediaServerDataEx&& data)
{
    StatisticsMediaServerData statisticsMediaServerData;
    (MediaServerDataEx&) statisticsMediaServerData = std::move(data);

    const static std::set<QString> kServerParamsToRemove =
    {
        nx::analytics::kPluginDescriptorsProperty,
        nx::analytics::kEngineDescriptorsProperty,
        nx::analytics::kGroupDescriptorsProperty,
        nx::analytics::kEventTypeDescriptorsProperty,
        nx::analytics::kObjectTypeDescriptorsProperty,
        nx::analytics::kDeviceDescriptorsProperty,
    };

    nx::utils::remove_if(
        statisticsMediaServerData.addParams,
        [](const auto& param) { return kServerParamsToRemove.count(param.name); });

    for (auto& s: statisticsMediaServerData.MediaServerDataEx::storages)
        statisticsMediaServerData.storages.push_back(toStatisticsData(std::move(s)));

    statisticsMediaServerData.MediaServerDataEx::storages.clear();

    return statisticsMediaServerData;
}

StatisticsLicenseData toStatisticsData(const LicenseData& data)
{
    StatisticsLicenseData statisticsLicenseData;

    QMap<QString, QString> parsed;
    for (const auto& value: data.licenseBlock.split('\n'))
    {
        auto pair = value.split('=');
        if (pair.size() == 2)
            parsed.insert(QLatin1String(pair[0]), QLatin1String(pair[1]));
    }

    statisticsLicenseData.name = parsed[lit("NAME")];
    statisticsLicenseData.key = parsed[lit("SERIAL")];
    statisticsLicenseData.licenseType = parsed[lit("CLASS")];
    statisticsLicenseData.cameraCount = parsed[lit("COUNT")].toLongLong();
    statisticsLicenseData.version = parsed[lit("VERSION")];
    statisticsLicenseData.brand = parsed[lit("BRAND")];
    statisticsLicenseData.expiration = parsed[lit("EXPIRATION")];

    return statisticsLicenseData;
}

StatisticsEventRuleData toStatisticsData(EventRuleData&& data)
{
    StatisticsEventRuleData statisticsEventRuleData;
    (EventRuleData&) statisticsEventRuleData = std::move(data);

    return statisticsEventRuleData;
}

StatisticsUserData toStatisticsData(UserData&& data)
{
    StatisticsUserData statisticsUserData;
    (UserData&) statisticsUserData = std::move(data);

    return statisticsUserData;
}

StatisticsPluginInfo toStatisticsData(PluginInfoEx&& data)
{
    StatisticsPluginInfo statisticsPluginInfo;
    (PluginInfoEx&) statisticsPluginInfo = std::move(data);

    return statisticsPluginInfo;
}

} // namespace nx::vms::server::statistics
