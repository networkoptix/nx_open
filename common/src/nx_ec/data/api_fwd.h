#ifndef QN_EC2_API_FWD_H
#define QN_EC2_API_FWD_H

#include <vector>

#include <utils/common/model_functions_fwd.h>

namespace ec2 {
    struct ApiData;

    struct ApiBusinessActionData;
    struct ApiBusinessRuleData;
    struct ApiCameraData;
    struct ApiCameraServerItemData;
    struct ApiEmailData;
    struct ApiEmailSettingsData;
    struct ApiFullInfoData;
    struct ApiIdData;
    struct ApiLayoutItemData;
    struct ApiLayoutData;
    struct ApiLicenseData;
    struct ApiLockData;
    struct ApiMediaServerData;
    struct ApiPanicModeData;
    struct ApiPropertyTypeData;
    struct ApiResourceData;
    struct ApiResourceParamData;
    struct ApiResourceParamsData;
    struct ApiResourceTypeData;
    struct ApiRuntimeData;
    struct ApiScheduleTaskData;
    struct ApiServerAliveData;
    struct ApiServerInfoData;
    struct ApiSetResourceDisabledData;
    struct ApiSetResourceStatusData;
    struct ApiStorageData;
    struct ApiStoredFileData;
    struct ApiUserData;
    struct ApiVideowallControlMessageData;
    struct ApiVideowallData;
    struct ApiVideowallItemData;
    struct ApiVideowallScreenData;

    typedef std::vector<ApiBusinessRuleData> ApiBusinessRuleDataList;
    typedef std::vector<ApiCameraData> ApiCameraDataList;
    typedef std::vector<ApiCameraServerItemData> ApiCameraServerItemDataList;
    typedef std::vector<ApiLayoutData> ApiLayoutDataList;
    typedef std::vector<ApiLicenseData> ApiLicenseDataList;
    typedef std::vector<ApiMediaServerData> ApiMediaServerDataList;
    typedef std::vector<ApiPropertyTypeData> ApiPropertyTypeDataList;
    typedef std::vector<ApiResourceParamData> ApiResourceParamDataList; // TODO: 
    typedef std::vector<ApiResourceTypeData> ApiResourceTypeDataList;
    typedef std::vector<ApiStorageData> ApiStorageDataList;
    typedef std::vector<ApiUserData> ApiUserDataList;
    typedef std::vector<ApiVideowallData> ApiVideowallDataList;

#define QN_EC2_API_DATA_CLASSES \
    (ApiBusinessActionData)\
    (ApiBusinessRuleData)\
    (ApiCameraData)\
    (ApiCameraServerItemData)\
    (ApiEmailData)\
    (ApiEmailSettingsData)\
    (ApiFullInfoData)\
    (ApiIdData)\
    (ApiLayoutItemData)\
    (ApiLayoutData)\
    (ApiLicenseData)\
    (ApiLockData)\
    (ApiMediaServerData)\
    (ApiPanicModeData)\
    (ApiPropertyTypeData)\
    (ApiResourceData)\
    (ApiResourceParamData)\
    (ApiResourceParamsData)\
    (ApiResourceTypeData)\
    (ApiRuntimeData)\
    (ApiScheduleTaskData)\
    (ApiServerAliveData)\
    (ApiServerInfoData)\
    (ApiSetResourceDisabledData)\
    (ApiSetResourceStatusData)\
    (ApiStorageData)\
    (ApiStoredFileData)\
    (ApiUserData)\
    (ApiVideowallControlMessageData)\
    (ApiVideowallData)\
    (ApiVideowallItemData)\
    (ApiVideowallScreenData)\

    QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
        QN_EC2_API_DATA_CLASSES,
        (binary)(json)
    );

} // namespace ec2

#define QN_QUERY_TO_DATA_OBJECT(...) // TODO: #EC2
#define QN_QUERY_TO_DATA_OBJECT_FILTERED(...) // TODO: #EC2

#endif // QN_EC2_API_FWD_H
