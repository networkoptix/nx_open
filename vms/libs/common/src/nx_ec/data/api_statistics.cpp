#include "api_statistics.h"

#include <core/resource/param.h>
#include <core/resource/resource_type.h>

#include <nx/fusion/model_functions.h>

using namespace nx::vms;

const static QString __CAMERA_EXCEPT_PARAMS[] =
{
    ResourcePropertyKey::kCredentials,
    ResourcePropertyKey::kDefaultCredentials,
    ResourcePropertyKey::kCameraAdvancedParams,
    ResourcePropertyKey::Onvif::kDeviceID,
    ResourcePropertyKey::Onvif::kDeviceUrl,
    ResourcePropertyKey::Onvif::kMediaUrl
};

const static QString __CAMERA_RESOURCE_PARAMS[] =
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

// TODO: remove this hack when VISUAL STUDIO supports initializer lists
#define INIT_LIST(array) &array[0], &array[(sizeof(array)/sizeof(array[0]))]

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

    // remove confidential information
    auto rm = std::remove_if(addParams.begin(), addParams.end(),
                                [](const auto& param)
                                { return EXCEPT_PARAMS.count(param.name); });

    addParams.erase(rm, addParams.end());
    addParams.push_back(nx::vms::api::ResourceParamData(defCred, isDefCred
        ? lit("true")
        : lit("false")));

    // update resource defaults if not in addParams
    for (const auto& param : RESOURCE_PARAMS)
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

const std::set<QString> ApiCameraDataStatistics::EXCEPT_PARAMS(
        INIT_LIST(__CAMERA_EXCEPT_PARAMS));

const std::set<QString> ApiCameraDataStatistics::RESOURCE_PARAMS(
        INIT_LIST(__CAMERA_RESOURCE_PARAMS));

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
