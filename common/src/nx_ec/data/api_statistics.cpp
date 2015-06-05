#include "api_statistics.h"
#include "api_model_functions_impl.h"
#include "core/resource/param.h"

#include <utils/serialization/lexical.h>

#include <unordered_map>

const static QString __CAMERA_EXCEPT_PARAMS[] = {
	Qn::CAMERA_CREDENTIALS_PARAM_NAME,
    Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME, 
//     Qn::CAMERA_SETTINGS_ID_PARAM_NAME,
//     Qn::PHYSICAL_CAMERA_SETTINGS_XML_PARAM_NAME,
	Qn::CAMERA_ADVANCED_PARAMETERS,
    QLatin1String("DeviceID"), QLatin1String("DeviceUrl"), // from plugin onvif
    QLatin1String("MediaUrl"),
};

// TODO: remove this hack when VISUAL STUDIO supports initializer lists
#define INIT_LIST(array) &array[0], &array[(sizeof(array)/sizeof(array[0]))]

namespace ec2 {

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES( \
            (ApiCameraDataStatistics)(ApiStorageDataStatistics)(ApiMediaServerDataStatistics) \
            (ApiLicenseStatistics)(ApiBusinessRuleStatistics) \
			(ApiSystemStatistics)(ApiStatisticsServerInfo), \
            (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

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
    }

    const std::unordered_set<QString> ApiCameraDataStatistics::EXCEPT_PARAMS(
            INIT_LIST(__CAMERA_EXCEPT_PARAMS));

    ApiStorageDataStatistics::ApiStorageDataStatistics() {}

    ApiStorageDataStatistics::ApiStorageDataStatistics(ApiStorageData&& data)
        : ApiStorageData(std::move(data))
	{}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics() {}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics(ApiMediaServerDataEx&& data)
        : ApiMediaServerDataEx(std::move(data))
    {
        for (auto s : ApiMediaServerDataEx::storages)
            storages.push_back(std::move(s));
    }

    ApiLicenseStatistics::ApiLicenseStatistics()
        : cameraCount(0) {}

    ApiLicenseStatistics::ApiLicenseStatistics(const ApiLicenseData& data)
        : cameraCount(0)
    {
        QMap<QString, QString> parsed;
        for (auto value : data.licenseBlock.split('\n'))
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

} // namespace ec2
