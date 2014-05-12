#ifndef QN_EC2_API_FWD_H
#define QN_EC2_API_FWD_H

#include <vector>

#include <utils/common/model_functions_fwd.h>

class QString;

namespace ec2 {
    struct ResourceContext;
    struct QnFullResourceData; // TODO: #Elric move these out?


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
    struct ApiLayoutItemWithRefData;
    struct ApiLayoutData;
    struct ApiLicenseData;
    struct ApiLockData;
    struct ApiMediaServerData;
    struct ApiPanicModeData;
    struct ApiPropertyTypeData;
    struct ApiResetBusinessRuleData;
    struct ApiResourceData;
    struct ApiResourceParamData;
    struct ApiResourceParamWithRefData;
    struct ApiResourceParamsData;
    struct ApiResourceTypeData;
    struct ApiRuntimeData;
    struct ApiScheduleTaskData;
    struct ApiScheduleTaskWithRefData;
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
    struct ApiVideowallItemWithRefData;
    struct ApiVideowallScreenData;
    struct ApiVideowallScreenWithRefData;
    struct ApiUpdateUploadData;
    struct ApiUpdateUploadResponceData;
    struct ApiCameraBookmarkTagData;

    typedef std::vector<ApiBusinessRuleData> ApiBusinessRuleDataList;
    typedef std::vector<ApiCameraData> ApiCameraDataList;
    typedef std::vector<ApiCameraServerItemData> ApiCameraServerItemDataList;
    typedef std::vector<ApiLayoutData> ApiLayoutDataList;
    typedef std::vector<ApiLicenseData> ApiLicenseDataList;
    typedef std::vector<ApiMediaServerData> ApiMediaServerDataList;
    typedef std::vector<ApiPropertyTypeData> ApiPropertyTypeDataList;
    typedef std::vector<ApiResourceData> ApiResourceDataList;
    typedef std::vector<ApiResourceParamData> ApiResourceParamDataList; // TODO: 
    typedef std::vector<ApiResourceTypeData> ApiResourceTypeDataList;
    typedef std::vector<ApiStorageData> ApiStorageDataList;
    typedef std::vector<ApiUserData> ApiUserDataList;
    typedef std::vector<ApiVideowallData> ApiVideowallDataList;
    typedef std::vector<ApiCameraBookmarkTagData> ApiCameraBookmarkTagDataList;

    typedef QString ApiStoredFilePath; // TODO: #Elric struct => extendable?
    typedef QString ApiUpdateInstallData; // TODO: #Elric struct => extendable?
    typedef std::vector<ApiStoredFilePath> ApiStoredDirContents;
    struct LoginInfo;

#define QN_EC2_API_DATA_TYPES \
    (ApiBusinessActionData)\
    (ApiBusinessRuleData)\
    (ApiCameraData)\
    (ApiCameraServerItemData)\
    (ApiEmailData)\
    (ApiEmailSettingsData)\
    (ApiFullInfoData)\
    (ApiIdData)\
    (ApiLayoutItemData)\
    (ApiLayoutItemWithRefData)\
    (ApiLayoutData)\
    (ApiLicenseData)\
    (ApiLockData)\
    (ApiMediaServerData)\
    (ApiPanicModeData)\
    (ApiPropertyTypeData)\
    (ApiResetBusinessRuleData)\
    (ApiResourceData)\
    (ApiResourceParamData)\
    (ApiResourceParamWithRefData)\
    (ApiResourceParamsData)\
    (ApiResourceTypeData)\
    (ApiRuntimeData)\
    (ApiScheduleTaskData)\
    (ApiScheduleTaskWithRefData)\
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
    (ApiVideowallItemWithRefData)\
    (ApiVideowallScreenData)\
    (ApiVideowallScreenWithRefData)\
    (ApiUpdateUploadData)\
    (ApiUpdateUploadResponceData)\
    (ApiCameraBookmarkTagData)\

#ifndef QN_NO_BASE
    QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
        QN_EC2_API_DATA_TYPES,
        (binary)(json)(sql_record)(csv_record)
    );
#endif
    
} // namespace ec2

#endif // QN_EC2_API_FWD_H
