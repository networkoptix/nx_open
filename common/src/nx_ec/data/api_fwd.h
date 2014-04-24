#ifndef QN_EC2_API_FWD_H
#define QN_EC2_API_FWD_H

#include <utils/common/model_functions_fwd.h>

namespace ec2 {
    struct ApiBusinessRuleData;
    struct ApiBusinessActionData;
    struct ApiScheduleTaskData;
    struct ApiCameraData;
    struct ApiCameraServerItemData;
    struct ApiData;
    struct ApiIdData;
    struct ApiEmailSettingsData;
    struct ApiEmailData;
    struct ApiFullInfoData;
    struct ApiLayoutItemData;
    struct ApiLayoutData;
    struct ApiLicenseData;
    struct ApiLockData;
    struct ApiStorageData;
    struct ApiMediaServerData;
    struct ApiPanicModeData;
    struct ApiResourceParamData;
    struct ApiResourceParamsData;
    struct ApiResourceData;
    struct ApiSetResourceDisabledData;
    struct ApiSetResourceStatusData;
    struct ApiPropertyTypeData;
    struct ApiResourceTypeData;
    struct ApiRuntimeData;
    struct ApiServerAliveData;
    struct ApiServerInfoData;
    struct ApiStoredFileData;
    struct ApiUserData;
    struct ApiVideowallItemData;
    struct ApiVideowallScreenData;
    struct ApiVideowallData;
    struct ApiVideowallControlMessageData;

    QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
        (ApiBusinessRuleData)(ApiBusinessActionData),
        (binary)(json)
    );

} // namespace ec2

#endif // QN_EC2_API_FWD_H
