#include "api_statistics.h"

#include <core/resource/param.h>
#include <core/resource/resource_type.h>

#include <nx/fusion/model_functions.h>

const static QString __CAMERA_EXCEPT_PARAMS[] =
{
	Qn::CAMERA_CREDENTIALS_PARAM_NAME,
    Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME,
	Qn::CAMERA_ADVANCED_PARAMETERS,
    QLatin1String("DeviceID"), QLatin1String("DeviceUrl"), // from plugin onvif
    QLatin1String("MediaUrl"),
};

const static QString __CAMERA_RESOURCE_PARAMS[] =
{
    Qn::ANALOG_PARAM_NAME,
    Qn::CAMERA_CAPABILITIES_PARAM_NAME,
    Qn::HAS_DUAL_STREAMING_PARAM_NAME,
    Qn::IS_AUDIO_SUPPORTED_PARAM_NAME,
    Qn::MAX_FPS_PARAM_NAME,
    Qn::PTZ_CAPABILITIES_PARAM_NAME,
    Qn::STREAM_FPS_SHARING_PARAM_NAME,
    Qn::SUPPORTED_MOTION_PARAM_NAME,
};

// TODO: remove this hack when VISUAL STUDIO supports initializer lists
#define INIT_LIST(array) &array[0], &array[(sizeof(array)/sizeof(array[0]))]

namespace ec2 {

    ApiCameraDataStatistics::ApiCameraDataStatistics() {}

    ApiCameraDataStatistics::ApiCameraDataStatistics(ApiCameraDataEx&& data)
        : ApiCameraDataEx(std::move(data))
    {
        // find out if default password worked
        const auto& defCred = Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME;
        const auto it = std::find_if(addParams.begin(), addParams.end(),
            [&defCred](const ApiResourceParamData& param) { return param.name == defCred; });
        const bool isDefCred = (it != ApiCameraDataEx::addParams.end()) && !it->value.isEmpty();

        // remove confidential information
        auto rm = std::remove_if(addParams.begin(), addParams.end(),
                                 [](const ec2::ApiResourceParamData& param)
                                 { return EXCEPT_PARAMS.count(param.name); });

        addParams.erase(rm, addParams.end());
        addParams.push_back(ApiResourceParamData(defCred, isDefCred ? lit("true") : lit("false")));

        // update resource defaults if not in addParams
        for (const auto& param : RESOURCE_PARAMS)
        {
            if (!qnResTypePool)
                return;

            const auto it = std::find_if(
                addParams.begin(), addParams.end(),
                [&param](ApiResourceParamData& d) { return d.name == param; });

            if (it != addParams.end() && !it->value.isEmpty() && it->value != lit("0"))
                continue;

            if (const auto type = qnResTypePool->getResourceType(typeId))
            {
                const auto value = type->defaultValue(param);
                if (!value.isEmpty())
                    addParams.push_back(ApiResourceParamData(param, value));
            }
        }
    }

    const std::set<QString> ApiCameraDataStatistics::EXCEPT_PARAMS(
            INIT_LIST(__CAMERA_EXCEPT_PARAMS));

    const std::set<QString> ApiCameraDataStatistics::RESOURCE_PARAMS(
            INIT_LIST(__CAMERA_RESOURCE_PARAMS));

    ApiStorageDataStatistics::ApiStorageDataStatistics() {}

    ApiStorageDataStatistics::ApiStorageDataStatistics(ApiStorageData&& data)
        : ApiStorageData(std::move(data))
    {}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics() {}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics(ApiMediaServerDataEx&& data)
        : ApiMediaServerDataEx(std::move(data))
    {
        for (auto& s : ApiMediaServerDataEx::storages)
            storages.push_back(std::move(s));

        ApiMediaServerDataEx::storages.clear();
    }

    ApiLicenseStatistics::ApiLicenseStatistics()
        : cameraCount(0) {}

    ApiLicenseStatistics::ApiLicenseStatistics(const ApiLicenseData& data)
        : cameraCount(0)
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

    ApiBusinessRuleStatistics::ApiBusinessRuleStatistics() {}

    ApiBusinessRuleStatistics::ApiBusinessRuleStatistics(ApiBusinessRuleData&& data)
        : ApiBusinessRuleData(std::move(data))
    {}

    ApiUserDataStatistics::ApiUserDataStatistics() {}

    ApiUserDataStatistics::ApiUserDataStatistics(ApiUserData&& data)
        : ApiUserData(std::move(data))
    {}

    ApiStatisticsReportInfo::ApiStatisticsReportInfo()
        : id(QnUuid::createUuid())
        , number(-1)
    {}

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
        API_STATISTICS_DATA_TYPES, (ubjson)(xml)(json)(csv_record), _Fields)

    } // namespace ec2
