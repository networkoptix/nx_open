#ifndef QN_EC2_API_FWD_H
#define QN_EC2_API_FWD_H

#include <vector>

#include <utils/common/model_functions_fwd.h>

class QString;
class QnUuid;

namespace ec2 {
    struct ApiData;

    struct ApiBusinessActionData;
    struct ApiBusinessRuleData;
    struct ApiCameraData;
    struct ApiCameraAttributesData;
    struct ApiCameraDataEx;
    struct ApiServerFootageData;
    struct ApiCameraHistoryItemData;
    struct ApiCameraHistoryData;
    struct ApiClientInfoData;
    struct ApiEmailData;
    struct ApiEmailSettingsData;
    struct ApiFullInfoData;
    struct ApiSyncMarkerRecord;
    struct ApiUpdateSequenceData;
    struct ApiIdData;
    struct ApiLayoutItemData;
    struct ApiLayoutItemWithRefData;
    struct ApiLayoutData;
    struct ApiLicenseData;
    struct ApiDetailedLicenseData;
    struct ApiLockData;
    struct ApiMediaServerData;
    struct ApiMediaServerUserAttributesData;
    struct ApiMediaServerDataEx;
    struct ApiPropertyTypeData;
    struct ApiResetBusinessRuleData;
    struct ApiResourceData;
    struct ApiResourceParamData;
    struct ApiResourceParamWithRefData;
    struct ApiResourceTypeData;
    struct ApiScheduleTaskData;
    struct ApiScheduleTaskWithRefData;
    struct QnTranStateKey;
    struct QnTranState;
    struct QnTranStateResponse;
    struct ApiSyncRequestData;
    struct ApiTranSyncDoneData;
    struct ApiPeerAliveData;
    struct ApiResourceStatusData;
    struct ApiReverseConnectionData;
    struct ApiStorageData;
    struct ApiStoredFileData;
    struct ApiStoredFilePath;
    struct ApiUserData;
    struct ApiAccessRightsData;
    struct ApiVideowallControlMessageData;
    struct ApiVideowallData;
    struct ApiVideowallItemData;
    struct ApiVideowallItemWithRefData;
    struct ApiVideowallScreenData;
    struct ApiVideowallScreenWithRefData;
    struct ApiVideowallMatrixData;
    struct ApiVideowallMatrixWithRefData;
    struct ApiVideowallMatrixItemData;
    struct ApiVideowallMatrixItemWithRefData;
    struct ApiUpdateUploadData;
    struct ApiUpdateUploadResponceData;
    struct ApiUpdateInstallData;
    struct ApiLoginData;
    struct ApiDiscoveryData;
    struct ApiDiscoverPeerData;
    struct ApiConnectionData;
    struct ApiSystemNameData;
    struct ApiTransactionData;
    struct ApiWebPageData;
    struct ApiDiscoveredServerData;

    struct ApiTimeData;
    struct ApiPeerSystemTimeData;
    typedef std::vector<ApiPeerSystemTimeData> ApiPeerSystemTimeDataList;

    struct ApiPeerData;
    struct ApiRuntimeData;

    struct ApiDatabaseDumpData;
    struct ApiLicenseOverflowData;

    typedef std::vector<ApiTransactionData> ApiTransactionDataList;
    typedef std::vector<ApiStoredFileData> ApiStoredFileDataList;
    typedef std::vector<ApiBusinessRuleData> ApiBusinessRuleDataList;
    typedef std::vector<ApiCameraData> ApiCameraDataList;
    typedef std::vector<ApiCameraAttributesData> ApiCameraAttributesDataList;
    typedef std::vector<ApiCameraDataEx> ApiCameraDataExList;
    typedef std::vector<ApiClientInfoData> ApiClientInfoDataList;
    typedef std::vector<ApiLayoutData> ApiLayoutDataList;
    typedef std::vector<ApiLicenseData> ApiLicenseDataList;
    typedef std::vector<ApiDetailedLicenseData> ApiDetailedLicenseDataList;
    typedef std::vector<ApiMediaServerData> ApiMediaServerDataList;
    typedef std::vector<ApiMediaServerUserAttributesData> ApiMediaServerUserAttributesDataList;
    typedef std::vector<ApiMediaServerDataEx> ApiMediaServerDataExList;
    typedef std::vector<ApiPropertyTypeData> ApiPropertyTypeDataList;
    typedef std::vector<ApiResourceData> ApiResourceDataList;
    typedef std::vector<ApiResourceParamData> ApiResourceParamDataList;
    typedef std::vector<ApiResourceTypeData> ApiResourceTypeDataList;
    typedef std::vector<ApiStorageData> ApiStorageDataList;
    typedef std::vector<ApiUserData> ApiUserDataList;
    typedef std::vector<ApiAccessRightsData> ApiAccessRightsDataList;
    typedef std::vector<ApiVideowallData> ApiVideowallDataList;
    typedef std::vector<ApiDiscoveryData> ApiDiscoveryDataList;
    typedef std::vector<ApiStoredFilePath> ApiStoredDirContents;
    typedef std::vector<ApiResourceParamWithRefData> ApiResourceParamWithRefDataList;
    typedef std::vector<ApiStorageData> ApiStorageDataList;
    typedef std::vector<ApiIdData> ApiIdDataList;
    typedef std::vector<ApiResourceStatusData> ApiResourceStatusDataList;
    typedef std::vector<ApiServerFootageData> ApiServerFootageDataList;
    typedef std::vector<ApiCameraHistoryData> ApiCameraHistoryDataList;
    typedef std::vector<ApiCameraHistoryItemData> ApiCameraHistoryItemDataList;
    typedef std::vector<ApiWebPageData> ApiWebPageDataList;
    typedef std::vector<ApiDiscoveredServerData> ApiDiscoveredServerDataList;

#define QN_EC2_API_DATA_TYPES \
    (ApiBusinessActionData)\
    (ApiBusinessRuleData)\
    (ApiCameraData)\
    (ApiServerFootageData)\
    (ApiCameraHistoryItemData)\
    (ApiCameraHistoryData)\
    (ApiCameraAttributesData)\
    (ApiCameraDataEx)\
    (ApiClientInfoData)\
    (ApiEmailData)\
    (ApiEmailSettingsData)\
    (ApiFullInfoData)\
    (ApiIdData)\
    (ApiSyncMarkerRecord)\
    (ApiUpdateSequenceData)\
    (ApiLayoutItemData)\
    (ApiLayoutItemWithRefData)\
    (ApiLayoutData)\
    (ApiLicenseData)\
    (ApiDetailedLicenseData)\
    (ApiLockData)\
    (ApiMediaServerData)\
    (ApiMediaServerUserAttributesData)\
    (ApiMediaServerDataEx)\
    (ApiPeerSystemTimeData)\
    (ApiPropertyTypeData)\
    (ApiResetBusinessRuleData)\
    (ApiReverseConnectionData)\
    (ApiResourceData)\
    (ApiResourceParamData)\
    (ApiResourceParamWithRefData)\
    (ApiResourceTypeData)\
    (ApiScheduleTaskData)\
    (ApiScheduleTaskWithRefData)\
    (QnTranStateKey)\
    (QnTranState)\
    (ApiSyncRequestData)\
    (QnTranStateResponse)\
    (ApiTranSyncDoneData)\
    (ApiPeerAliveData)\
    (ApiResourceStatusData)\
    (ApiStoredFilePath)\
    (ApiStorageData)\
    (ApiStoredFileData)\
    (ApiUserData)\
    (ApiAccessRightsData)\
    (ApiVideowallControlMessageData)\
    (ApiVideowallData)\
    (ApiVideowallItemData)\
    (ApiVideowallItemWithRefData)\
    (ApiVideowallScreenData)\
    (ApiVideowallScreenWithRefData)\
    (ApiVideowallMatrixData)\
    (ApiVideowallMatrixWithRefData)\
    (ApiVideowallMatrixItemData)\
    (ApiVideowallMatrixItemWithRefData)\
    (ApiUpdateInstallData)\
    (ApiUpdateUploadData)\
    (ApiUpdateUploadResponceData)\
    (ApiLoginData)\
    (ApiDiscoveryData)\
    (ApiDiscoverPeerData)\
    (ApiConnectionData)\
    (ApiSystemNameData)\
    (ApiTimeData)\
    (ApiPeerData)\
    (ApiRuntimeData)\
    (ApiDatabaseDumpData)\
    (ApiLicenseOverflowData)\
    (ApiWebPageData)\
    (ApiDiscoveredServerData)\

#ifndef QN_NO_BASE
    QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
        QN_EC2_API_DATA_TYPES,
        (ubjson)(xml)(json)(sql_record)(csv_record)
    );
#endif

} // namespace ec2

#endif // QN_EC2_API_FWD_H
