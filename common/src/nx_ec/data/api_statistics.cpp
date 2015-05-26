#include "api_statistics.h"
#include "api_model_functions_impl.h"
#include "core/resource/param.h"

#include <utils/serialization/lexical.h>

#include <unordered_map>

const static QString __CAMERA_EXCEPT_PARAMS[] = {
	Qn::CAMERA_CREDENTIALS_PARAM_NAME,
    Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME, 
    Qn::CAMERA_SETTINGS_ID_PARAM_NAME,
    Qn::PHYSICAL_CAMERA_SETTINGS_XML_PARAM_NAME,
    QLatin1String("DeviceID"), QLatin1String("DeviceUrl"), // from plugin onvif
    QLatin1String("MediaUrl"),
};

// TODO: remove this hack when VISUAL STUDIO supports initializer lists
#define INIT_LIST(array) &array[0], &array[(sizeof(array)/sizeof(array[0]))]

static ec2::ApiResourceParamDataList getExtraParams( const ec2::ApiResourceParamDataList& paramList,
                                                     const std::unordered_set<QString>& exceptSet)
{
    ec2::ApiResourceParamDataList extraParams;
    for (auto& param : paramList)
        if (!exceptSet.count(param.name))
            extraParams.push_back(param);
    return extraParams;
}

namespace ec2 {

    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES( \
            (ApiCameraDataStatistics)(ApiStorageDataStatistics)(ApiMediaServerDataStatistics) \
            (ApiLicenseStatistics)(ApiBusinessRuleStatistics) \
			(ApiSystemStatistics)(ApiStatisticsServerInfo), \
            (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

    ApiCameraDataStatistics::ApiCameraDataStatistics() {}

    ApiCameraDataStatistics::ApiCameraDataStatistics(const ApiCameraDataEx&& data)
        : ApiCameraDataEx(data)
        , addParams(getExtraParams(ApiCameraDataEx::addParams, EXCEPT_PARAMS)) {}

	const std::unordered_set<QString> ApiCameraDataStatistics::EXCEPT_PARAMS(INIT_LIST(__CAMERA_EXCEPT_PARAMS));

    ApiStorageDataStatistics::ApiStorageDataStatistics() {}

    ApiStorageDataStatistics::ApiStorageDataStatistics(const ApiStorageData&& data)
		: ApiStorageData(data) 
	{}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics() {}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics(const ApiMediaServerDataEx&& data)
        : ApiMediaServerDataEx(data)
    {
        for (auto s : ApiMediaServerDataEx::storages)
            storages.push_back(std::move(s));
    }

    ApiLicenseStatistics::ApiLicenseStatistics() {}

    ApiLicenseStatistics::ApiLicenseStatistics(const ApiLicenseData& data)
    {
        QMap<QString, QString> parsed;
        for (auto value : data.licenseBlock.split('\n'))
        {
            auto pair = value.split('=');
            if (pair.size() == 2) 
                parsed.insert(QLatin1String(pair[0]), QLatin1String(pair[1]));
        }

        name        = parsed[lit("NAME")];
		licenseType = parsed[lit("CLASS")];
        cameraCount = parsed[lit("COUNT")].toLongLong();
        version     = parsed[lit("VERSION")];
        brand       = parsed[lit("BRAND")];
        expiration  = parsed[lit("EXPIRATION")];
    }

	ApiBusinessRuleStatistics::ApiBusinessRuleStatistics() {}

    ApiBusinessRuleStatistics::ApiBusinessRuleStatistics(const ApiBusinessRuleData&& data)
		: ApiBusinessRuleData(data)
	{}

} // namespace ec2
