#include "api_statistics.h"
#include "api_model_functions_impl.h"

#include <unordered_set>

namespace std
{
    template<> struct hash<QString>
    {
        std::size_t operator()(const QString& s) const
        {
            return qHash(s);
        }
    };
}

const static std::unordered_set<QString> CAMERA_EXTRA_PARAMS = {
    lit("MaxFPS"), lit("cameraSettingsId"), lit("firmware"), lit("hasDualStreaming"),
    lit("isAudioSupported"), lit("mediaStreams"), lit("ptzCapabilities"),
};
const static std::unordered_set<QString> CLIENT_EXTRA_PARAMS = {
    lit("cpuArchitecture"), lit("cpuModelName"), lit("phisicalMemory"), lit("videoCard"), lit("videoDriver")
};
const static std::unordered_set<QString> MEDIASERVER_EXTRA_PARAMS = {
    lit("cpuArchitecture"), lit("cpuModelName"), lit("phisicalMemory")
};

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES( \
            (ApiCameraDataStatistics)(ApiClientDataStatistics) \
            (ApiStorageDataStatistics)(ApiMediaServerDataStatistics) \
            (ApiSystemStatistics)(ApiStatisticsServerInfo), \
            (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

    static ApiResourceParamDataList getExtraParams(
            const ApiResourceParamDataList& paramList,
            const std::unordered_set<QString>& nameSet)
    {
        ApiResourceParamDataList extraParams;
        for (auto& param : paramList)
            if (nameSet.count(param.name))
                extraParams.push_back(param);
        return extraParams;
    }

    ApiCameraDataStatistics::ApiCameraDataStatistics() {}

    ApiCameraDataStatistics::ApiCameraDataStatistics(const ApiCameraDataEx& data)
        : ApiCameraDataEx(data)
        , addParams(getExtraParams(ApiCameraDataEx::addParams, CAMERA_EXTRA_PARAMS)) {}

    ApiClientDataStatistics::ApiClientDataStatistics() {}

    ApiClientDataStatistics::ApiClientDataStatistics(const ApiCameraDataEx& data)
        : ApiCameraDataEx(data)
        , addParams(getExtraParams(ApiCameraDataEx::addParams, CLIENT_EXTRA_PARAMS)) {}

    ApiStorageDataStatistics::ApiStorageDataStatistics() {}

    ApiStorageDataStatistics::ApiStorageDataStatistics(const ApiStorageData& data)
        : ApiStorageData(data) {}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics() {}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics(const ApiMediaServerDataEx& data)
        : ApiMediaServerDataEx(data)
        , addParams(getExtraParams(ApiMediaServerDataEx::addParams, MEDIASERVER_EXTRA_PARAMS))
    {
        for (auto s : ApiMediaServerDataEx::storages)
            storages.push_back(s);
    }

} // namespace ec2
