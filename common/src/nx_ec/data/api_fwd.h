#pragma once

#include <nx/vms/api/data_fwd.h>

#include <nx/utils/uuid.h>

class QString;

namespace ec2 {

using ApiDataWithVersion = nx::vms::api::DataWithVersion;
using ApiDatabaseDumpData = nx::vms::api::DatabaseDumpData;
using ApiDatabaseDumpToFileData = nx::vms::api::DatabaseDumpToFileData;
using ApiSyncMarkerRecord = nx::vms::api::SyncMarkerRecordData;
using ApiBusinessRuleData = nx::vms::api::EventRuleData;
using ApiBusinessRuleDataList = nx::vms::api::EventRuleDataList;
using ApiBusinessActionData = nx::vms::api::EventActionData;
using ApiResetBusinessRuleData = nx::vms::api::ResetEventRulesData;
using ApiResourceData = nx::vms::api::ResourceData;
using ApiResourceDataList = nx::vms::api::ResourceDataList;
using ApiResourceParamData = nx::vms::api::ResourceParamData;
using ApiResourceParamDataList = nx::vms::api::ResourceParamDataList;
using ApiResourceParamWithRefData = nx::vms::api::ResourceParamWithRefData;
using ApiResourceParamWithRefDataList = nx::vms::api::ResourceParamWithRefDataList;
using ApiResourceStatusData = nx::vms::api::ResourceStatusData;
using ApiResourceStatusDataList = nx::vms::api::ResourceStatusDataList;
using ApiCameraData = nx::vms::api::CameraData;
using ApiCameraDataList = nx::vms::api::CameraDataList;
using ApiScheduleTaskData = nx::vms::api::ScheduleTaskData;
using ApiScheduleTaskWithRefData = nx::vms::api::ScheduleTaskWithRefData;
using ApiCameraAttributesData = nx::vms::api::CameraAttributesData;
using ApiCameraAttributesDataList = nx::vms::api::CameraAttributesDataList;
using ApiCameraDataEx = nx::vms::api::CameraDataEx;
using ApiCameraDataExList = nx::vms::api::CameraDataExList;
using ApiServerFootageData = nx::vms::api::ServerFootageData;
using ApiServerFootageDataList = nx::vms::api::ServerFootageDataList;
using ApiCameraHistoryItemData = nx::vms::api::CameraHistoryItemData;
using ApiCameraHistoryItemDataList = nx::vms::api::CameraHistoryItemDataList;
using ApiCameraHistoryData = nx::vms::api::CameraHistoryData;
using ApiCameraHistoryDataList = nx::vms::api::CameraHistoryDataList;
using ApiEmailSettingsData = nx::vms::api::EmailSettingsData;
using ApiUpdateSequenceData = nx::vms::api::UpdateSequenceData;


    struct ApiFullInfoData;
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


    struct ApiResourceTypeData;

    struct ApiPersistentIdData;
    struct QnTranState;
    struct QnTranStateResponse;
    struct ApiSyncRequestData;
    struct ApiTranSyncDoneData;
    struct ApiPeerAliveData;
    struct ApiReverseConnectionData;
    struct ApiStorageData;
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

    struct ApiLicenseOverflowData;
    struct ApiCleanupDatabaseData;

    struct ApiP2pStatisticsData;

    typedef std::vector<ApiTransactionData> ApiTransactionDataList;

    typedef std::vector<ApiLayoutData> ApiLayoutDataList;
    using ApiLayoutTourDataList = std::vector<ApiLayoutTourData>;
    using ApiLayoutTourItemDataList = std::vector<ApiLayoutTourItemData>;
    typedef std::vector<ApiLicenseData> ApiLicenseDataList;
    typedef std::vector<ApiDetailedLicenseData> ApiDetailedLicenseDataList;
    typedef std::vector<ApiMediaServerData> ApiMediaServerDataList;
    typedef std::vector<ApiMediaServerUserAttributesData> ApiMediaServerUserAttributesDataList;
    typedef std::vector<ApiMediaServerDataEx> ApiMediaServerDataExList;
    typedef std::vector<ApiPropertyTypeData> ApiPropertyTypeDataList;
    typedef std::vector<ApiResourceTypeData> ApiResourceTypeDataList;
    typedef std::vector<ApiStorageData> ApiStorageDataList;
    typedef std::vector<ApiUserData> ApiUserDataList;
    typedef std::vector<ApiUserRoleData> ApiUserRoleDataList;
    typedef std::vector<ApiPredefinedRoleData> ApiPredefinedRoleDataList;
    typedef std::vector<ApiAccessRightsData> ApiAccessRightsDataList;
    typedef std::vector<ApiVideowallData> ApiVideowallDataList;
    typedef std::vector<ApiDiscoveryData> ApiDiscoveryDataList;
    typedef std::vector<ApiStorageData> ApiStorageDataList;
    typedef std::vector<ApiWebPageData> ApiWebPageDataList;
    typedef std::vector<ApiDiscoveredServerData> ApiDiscoveredServerDataList;
    typedef std::vector<ApiUpdateUploadResponceData> ApiUpdateUploadResponceDataList;
    typedef std::vector<ApiSystemMergeHistoryRecord> ApiSystemMergeHistoryRecordList;

    struct ApiEmailData;

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
    (ApiEmailData)\
    (ApiEmailSettingsData)\
    (ApiFullInfoData)\
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
    (ApiReverseConnectionData)\
    (ApiResourceTypeData)\
    (ApiPersistentIdData)\
    (QnTranState)\
    (ApiSyncRequestData)\
    (QnTranStateResponse)\
    (ApiTranSyncDoneData)\
    (ApiPeerAliveData)\
    (ApiStorageData)\
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
    (ApiLicenseOverflowData)\
    (ApiCleanupDatabaseData)\
    (ApiWebPageData)\
    (ApiDiscoveredServerData)\
    (ApiP2pStatisticsData)\

    QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
        QN_EC2_API_DATA_TYPES,
        (ubjson)(xml)(json)(sql_record)(csv_record)
    );

    QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
        (ApiUserData),
        (eq)
    );

} // namespace ec2
