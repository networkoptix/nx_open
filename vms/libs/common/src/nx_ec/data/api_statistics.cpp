#include "api_statistics.h"

#include <core/resource/camera_resource.h>
#include <core/resource/param.h>
#include <core/resource/resource_type.h>
#include <nx/analytics/properties.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/std/algorithm.h>

using namespace nx::vms;

const static std::set<QString> kCameraParamsToRemove =
{
    ResourcePropertyKey::kCredentials,
    ResourcePropertyKey::kDefaultCredentials,
    ResourcePropertyKey::kCameraAdvancedParams,
    ResourcePropertyKey::Onvif::kDeviceID,
    ResourcePropertyKey::Onvif::kDeviceUrl,
    ResourcePropertyKey::Onvif::kMediaUrl,
    QnVirtualCameraResource::kUserEnabledAnalyticsEnginesProperty,
    QnVirtualCameraResource::kCompatibleAnalyticsEnginesProperty,
    QnVirtualCameraResource::kDeviceAgentsSettingsValuesProperty,
    QnVirtualCameraResource::kDeviceAgentManifestsProperty,
};

const static std::set<QString> kServerParamsToRemove =
{
    nx::analytics::kPluginDescriptorsProperty,
    nx::analytics::kEngineDescriptorsProperty,
    nx::analytics::kGroupDescriptorsProperty,
    nx::analytics::kEventTypeDescriptorsProperty,
    nx::analytics::kObjectTypeDescriptorsProperty,
    nx::analytics::kDeviceDescriptorsProperty,
};

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

namespace ec2 {

ApiCameraDataStatistics::ApiCameraDataStatistics() {}

ApiCameraDataStatistics::ApiCameraDataStatistics(nx::vms::api::CameraDataEx&& data)
    : nx::vms::api::CameraDataEx(std::move(data))
{
    // find out if default password worked
    const auto& defCred = ResourcePropertyKey::kDefaultCredentials;
    const auto it = std::find_if(addParams.begin(), addParams.end(),
        [&defCred](const auto& param) { return param.name == defCred; });
    const bool isDefCred = (it != nx::vms::api::CameraDataEx::addParams.end()) && !it->value.isEmpty();

    nx::utils::remove_if(
        addParams, [](const auto& param) { return kCameraParamsToRemove.count(param.name); });

    addParams.push_back(nx::vms::api::ResourceParamData(defCred, isDefCred
        ? lit("true")
        : lit("false")));

    for (const auto& param : kCameraParamsToDefault)
    {
        if (!qnResTypePool)
            return;

        const auto it = std::find_if(
            addParams.begin(), addParams.end(),
            [&param](const auto& d) { return d.name == param; });

        if (it != addParams.end() && !it->value.isEmpty() && it->value != lit("0"))
            continue;

        if (const auto type = qnResTypePool->getResourceType(typeId))
        {
            const auto value = type->defaultValue(param);
            if (!value.isEmpty())
                addParams.push_back(nx::vms::api::ResourceParamData(param, value));
        }
    }
}

ApiStorageDataStatistics::ApiStorageDataStatistics()
{
}

ApiStorageDataStatistics::ApiStorageDataStatistics(api::StorageData&& data):
    api::StorageData(std::move(data))
{
}

ApiMediaServerDataStatistics::ApiMediaServerDataStatistics()
{
}

ApiMediaServerDataStatistics::ApiMediaServerDataStatistics(api::MediaServerDataEx&& data):
    api::MediaServerDataEx(std::move(data))
{
    nx::utils::remove_if(
        addParams, [](const auto& param) { return kServerParamsToRemove.count(param.name); });

    for (auto& s: api::MediaServerDataEx::storages)
        storages.push_back(std::move(s));

    api::MediaServerDataEx::storages.clear();
}

ApiLicenseStatistics::ApiLicenseStatistics(): cameraCount(0)
{
}

ApiLicenseStatistics::ApiLicenseStatistics(const nx::vms::api::LicenseData& data):
    cameraCount(0)
{
    QMap<QString, QString> parsed;
    for (const auto& value : data.licenseBlock.split('\n'))
    {
        auto pair = value.split('=');
        if (pair.size() == 2)
            parsed.insert(QLatin1String(pair[0]), QLatin1String(pair[1]));
    }

    name        = parsed[lit("NAME")];
    key         = parsed[lit("SERIAL")];
    licenseType = parsed[lit("CLASS")];
    cameraCount = parsed[lit("COUNT")].toLongLong();
    version     = parsed[lit("VERSION")];
    brand       = parsed[lit("BRAND")];
    expiration  = parsed[lit("EXPIRATION")];
}

ApiBusinessRuleStatistics::ApiBusinessRuleStatistics()
{
}

ApiBusinessRuleStatistics::ApiBusinessRuleStatistics(nx::vms::api::EventRuleData&& data):
    nx::vms::api::EventRuleData(std::move(data))
{
}

ApiUserDataStatistics::ApiUserDataStatistics()
{
}

ApiUserDataStatistics::ApiUserDataStatistics(nx::vms::api::UserData&& data):
    nx::vms::api::UserData(std::move(data))
{
}

ApiStatisticsReportInfo::ApiStatisticsReportInfo():
    id(QnUuid::createUuid()),
    number(-1)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    API_STATISTICS_DATA_TYPES, (ubjson)(xml)(json)(csv_record), _Fields)

} // namespace ec2
