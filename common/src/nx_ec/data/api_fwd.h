#ifndef QN_EC2_API_FWD_H
#define QN_EC2_API_FWD_H

#include <vector>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

class QString;

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
    struct ApiEmailData;
    struct ApiEmailSettingsData;
    struct ApiFullInfoData;
    struct ApiSyncMarkerRecord;
    struct ApiUpdateSequenceData;
    struct ApiIdData;
    struct ApiLayoutItemData;
    struct ApiLayoutData;
    struct ApiLayoutTourItemData;
    struct ApiLayoutTourData;
    struct ApiLayoutTourSettings;
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
    struct ApiPersistentIdData;
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
    struct ApiUserRoleData;
    struct ApiPredefinedRoleData;
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
    struct ApiSystemMergeHistoryRecord;
    struct ApiUpdateInstallData;
    struct ApiLoginData;
    struct ApiDiscoveryData;
    struct ApiDiscoverPeerData;
    struct ApiConnectionData;
    struct ApiSystemIdData;
    struct ApiTransactionData;
    struct ApiTranLogFilter;
    struct ApiWebPageData;
    struct ApiDiscoveredServerData;

    struct ApiTimeData;
    struct ApiMiscData;
    typedef std::vector<ApiMiscData> ApiMiscDataList;
    struct ApiPeerSystemTimeData;
    struct ApiPeerSyncTimeData;
    typedef std::vector<ApiPeerSystemTimeData> ApiPeerSystemTimeDataList;

    struct ApiPeerData;
    struct ApiPeerDataEx;
    struct ApiRuntimeData;

    struct ApiDatabaseDumpData;
    struct ApiDatabaseDumpToFileData;
    struct ApiLicenseOverflowData;
    struct ApiCleanupDatabaseData;

    struct ApiP2pStatisticsData;

    typedef std::vector<ApiTransactionData> ApiTransactionDataList;
    typedef std::vector<ApiStoredFileData> ApiStoredFileDataList;
    typedef std::vector<ApiBusinessRuleData> ApiBusinessRuleDataList;
    typedef std::vector<ApiCameraData> ApiCameraDataList;
    typedef std::vector<ApiCameraAttributesData> ApiCameraAttributesDataList;
    typedef std::vector<ApiCameraDataEx> ApiCameraDataExList;
    typedef std::vector<ApiLayoutData> ApiLayoutDataList;
    using ApiLayoutTourDataList = std::vector<ApiLayoutTourData>;
    using ApiLayoutTourItemDataList = std::vector<ApiLayoutTourItemData>;
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
    typedef std::vector<ApiUserRoleData> ApiUserRoleDataList;
    typedef std::vector<ApiPredefinedRoleData> ApiPredefinedRoleDataList;
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
    typedef std::vector<ApiUpdateUploadResponceData> ApiUpdateUploadResponceDataList;
    typedef std::vector<ApiSystemMergeHistoryRecord> ApiSystemMergeHistoryRecordList;

    /**
     * Wrapper to be used for overloading as a distinct type for ApiStorageData api requests.
     */
    struct ParentId
    {
        QnUuid id;
        ParentId() = default;
        ParentId(const QnUuid& id): id(id) {}
    };

#define QN_EC2_API_DATA_TYPES \
    (ApiBusinessActionData)\
    (ApiBusinessRuleData)\
    (ApiCameraData)\
    (ApiServerFootageData)\
    (ApiCameraHistoryItemData)\
    (ApiCameraHistoryData)\
    (ApiCameraAttributesData)\
    (ApiCameraDataEx)\
    (ApiEmailData)\
    (ApiEmailSettingsData)\
    (ApiFullInfoData)\
    (ApiIdData)\
    (ApiSyncMarkerRecord)\
    (ApiUpdateSequenceData)\
    (ApiLayoutItemData)\
    (ApiLayoutData)\
    (ApiLayoutTourItemData)\
    (ApiLayoutTourData)\
    (ApiLayoutTourSettings)\
    (ApiLicenseData)\
    (ApiDetailedLicenseData)\
    (ApiLockData)\
    (ApiMediaServerData)\
    (ApiMediaServerUserAttributesData)\
    (ApiMediaServerDataEx)\
    (ApiPeerSystemTimeData)\
    (ApiPeerSyncTimeData)\
    (ApiPropertyTypeData)\
    (ApiResetBusinessRuleData)\
    (ApiReverseConnectionData)\
    (ApiResourceData)\
    (ApiResourceParamData)\
    (ApiResourceParamWithRefData)\
    (ApiResourceTypeData)\
    (ApiScheduleTaskData)\
    (ApiScheduleTaskWithRefData)\
    (ApiPersistentIdData)\
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
    (ApiUserRoleData)\
    (ApiPredefinedRoleData)\
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
    (ApiSystemMergeHistoryRecord) \
    (ApiLoginData)\
    (ApiDiscoveryData)\
    (ApiDiscoverPeerData)\
    (ApiConnectionData)\
    (ApiSystemIdData)\
    (ApiTimeData)\
    (ApiMiscData)\
    (ApiPeerData)\
    (ApiPeerDataEx)\
    (ApiRuntimeData)\
    (ApiDatabaseDumpData)\
    (ApiDatabaseDumpToFileData)\
    (ApiLicenseOverflowData)\
    (ApiCleanupDatabaseData)\
    (ApiWebPageData)\
    (ApiDiscoveredServerData)\
    (ApiP2pStatisticsData)\

    QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
        QN_EC2_API_DATA_TYPES,
        (ubjson)(xml)(json)(sql_record)(csv_record)
    );

} // namespace ec2

#endif // QN_EC2_API_FWD_H
