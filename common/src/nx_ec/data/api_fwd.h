#pragma once

#include <nx/vms/api/data_fwd.h>

#include <nx/utils/uuid.h>

class QString;

namespace ec2 {

struct ApiFullInfoData;
struct ApiLicenseData;
struct ApiDetailedLicenseData;
struct ApiMediaServerData;
struct ApiMediaServerUserAttributesData;
struct ApiMediaServerDataEx;

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
struct ApiUpdateUploadData;
struct ApiUpdateUploadResponceData;
struct ApiSystemMergeHistoryRecord;
struct ApiUpdateInstallData;
struct ApiDiscoveryData;
struct ApiDiscoverPeerData;
struct ApiConnectionData;
struct ApiSystemIdData;
struct ApiTransactionData;
struct ApiTranLogFilter;
struct ApiDiscoveredServerData;

struct ApiTimeData;
struct ApiMiscData;
typedef std::vector<ApiMiscData> ApiMiscDataList;
struct ApiPeerSyncTimeData;

struct ApiPeerData;
struct ApiPeerDataEx;
struct ApiRuntimeData;

struct ApiLicenseOverflowData;
struct ApiCleanupDatabaseData;

struct ApiP2pStatisticsData;

typedef std::vector<ApiTransactionData> ApiTransactionDataList;

typedef std::vector<ApiLicenseData> ApiLicenseDataList;
typedef std::vector<ApiDetailedLicenseData> ApiDetailedLicenseDataList;
typedef std::vector<ApiMediaServerData> ApiMediaServerDataList;
typedef std::vector<ApiMediaServerUserAttributesData> ApiMediaServerUserAttributesDataList;
typedef std::vector<ApiMediaServerDataEx> ApiMediaServerDataExList;

typedef std::vector<ApiStorageData> ApiStorageDataList;
typedef std::vector<ApiUserData> ApiUserDataList;
typedef std::vector<ApiUserRoleData> ApiUserRoleDataList;
typedef std::vector<ApiPredefinedRoleData> ApiPredefinedRoleDataList;
typedef std::vector<ApiAccessRightsData> ApiAccessRightsDataList;
typedef std::vector<ApiDiscoveryData> ApiDiscoveryDataList;
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
    (ApiFullInfoData)\
    (ApiLicenseData)\
    (ApiDetailedLicenseData)\
    (ApiMediaServerData)\
    (ApiMediaServerUserAttributesData)\
    (ApiMediaServerDataEx)\
    (ApiPeerSyncTimeData)\
    (ApiReverseConnectionData)\
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
    (ApiUpdateInstallData)\
    (ApiUpdateUploadData)\
    (ApiUpdateUploadResponceData)\
    (ApiSystemMergeHistoryRecord) \
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
    (ApiDiscoveredServerData)\
    (ApiP2pStatisticsData)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    QN_EC2_API_DATA_TYPES,
    (ubjson)(xml)(json)(sql_record)(csv_record)
);

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ApiUserData),
    (eq)
);

} // namespace ec2
