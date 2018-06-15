#pragma once

#include <nx/vms/api/data_fwd.h>

#include <nx/utils/uuid.h>

class QString;

namespace ec2 {

struct ApiFullInfoData;
struct ApiMediaServerData;
struct ApiMediaServerUserAttributesData;
struct ApiMediaServerDataEx;

struct ApiStorageData;
struct ApiUserData;
struct ApiUserRoleData;
struct ApiPredefinedRoleData;
struct ApiSystemMergeHistoryRecord;
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

struct ApiRuntimeData;

typedef std::vector<ApiTransactionData> ApiTransactionDataList;

typedef std::vector<ApiMediaServerData> ApiMediaServerDataList;
typedef std::vector<ApiMediaServerUserAttributesData> ApiMediaServerUserAttributesDataList;
typedef std::vector<ApiMediaServerDataEx> ApiMediaServerDataExList;

typedef std::vector<ApiStorageData> ApiStorageDataList;
typedef std::vector<ApiUserData> ApiUserDataList;
typedef std::vector<ApiUserRoleData> ApiUserRoleDataList;
typedef std::vector<ApiPredefinedRoleData> ApiPredefinedRoleDataList;
typedef std::vector<ApiDiscoveryData> ApiDiscoveryDataList;
typedef std::vector<ApiDiscoveredServerData> ApiDiscoveredServerDataList;
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
    (ApiFullInfoData)\
    (ApiMediaServerData)\
    (ApiMediaServerUserAttributesData)\
    (ApiMediaServerDataEx)\
    (ApiStorageData)\
    (ApiUserData)\
    (ApiUserRoleData)\
    (ApiPredefinedRoleData)\
    (ApiSystemMergeHistoryRecord) \
    (ApiDiscoveryData)\
    (ApiDiscoverPeerData)\
    (ApiConnectionData)\
    (ApiSystemIdData)\
    (ApiTimeData)\
    (ApiMiscData)\
    (ApiRuntimeData)\
    (ApiDiscoveredServerData)\

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    QN_EC2_API_DATA_TYPES,
    (ubjson)(xml)(json)(sql_record)(csv_record)
);

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ApiUserData),
    (eq)
);

} // namespace ec2
