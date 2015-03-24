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
    lit("MaxFPS"), lit("firmware"), lit("hasDualStreaming"), lit("isAudioSupported")
};
const static std::unordered_set<QString> CLIENT_EXTRA_PARAMS = {
    lit("cpuArchitecture"), lit("cpuCoreCount"), lit("memory"), lit("videoChipset"), lit("videoMemory")
};
const static std::unordered_set<QString> MEDIASERVER_EXTRA_PARAMS = {
    lit("cpuArchitecture"), lit("cpuCoreCount"), lit("memory")
};

namespace ec2 {
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES( \
            (ApiCameraDataStatistics)(ApiClientDataStatistics)(ApiMediaServerDataStatistics) \
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
        , extraParams(getExtraParams(addParams, CAMERA_EXTRA_PARAMS)) {}

    ApiClientDataStatistics::ApiClientDataStatistics() {}

    ApiClientDataStatistics::ApiClientDataStatistics(const ApiCameraDataEx& data)
        : ApiCameraDataEx(data)
        , extraParams(getExtraParams(addParams, CLIENT_EXTRA_PARAMS)) {}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics() {}

    ApiMediaServerDataStatistics::ApiMediaServerDataStatistics(const ApiMediaServerDataEx& data)
        : ApiMediaServerDataEx(data)
        , extraParams(getExtraParams(addParams, MEDIASERVER_EXTRA_PARAMS)) {}

} // namespace ec2
