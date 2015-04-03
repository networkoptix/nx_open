#include "api_statistics.h"
#include "api_model_functions_impl.h"

#include <utils/serialization/lexical.h>

#include <unordered_map>

const static QString __CAMERA_DATA[] = {
	QLatin1String("MaxFPS"), QLatin1String("cameraSettingsId"), QLatin1String("firmware"),
	QLatin1String("hasDualStreaming"), QLatin1String("isAudioSupported"), 
	QLatin1String("mediaStreams"), QLatin1String("ptzCapabilities"),
};
const static QString __MEDIASERVER_DATA[] = {
    QLatin1String("cpuArchitecture"), QLatin1String("cpuModelName"), QLatin1String("phisicalMemory")
};

// TODO: remove this hack when VISUAL STUDIO supports initializer lists
#define INIT_LIST(array) &array[0], &array[(sizeof(array)/sizeof(array[0]))]

static ec2::ApiResourceParamDataList getExtraParams( const ec2::ApiResourceParamDataList& paramList,
                                                     const std::unordered_set<QString>& nameSet)
{
    ec2::ApiResourceParamDataList extraParams;
    for (auto& param : paramList)
        if (nameSet.count(param.name))
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
        , addParams(getExtraParams(ApiCameraDataEx::addParams, ADD_PARAMS)) {}

	const std::unordered_set<QString> ApiCameraDataStatistics::ADD_PARAMS(INIT_LIST(__CAMERA_DATA));

    ApiStorageDataStatistics::ApiStorageDataStatistics() {}

    ApiStorageDataStatistics::ApiStorageDataStatistics(const ApiStorageData&& data)
		: ApiStorageData(data) 
	{}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics() {}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics(const ApiMediaServerDataEx&& data)
        : ApiMediaServerDataEx(data)
        , addParams(getExtraParams(ApiMediaServerDataEx::addParams, ADD_PARAMS))
    {
        for (auto s : ApiMediaServerDataEx::storages)
            storages.push_back(std::move(s));
    }
	
	const std::unordered_set<QString> ApiMediaServerDataStatistics::ADD_PARAMS(INIT_LIST(__MEDIASERVER_DATA));

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
