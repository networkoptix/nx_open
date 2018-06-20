#pragma once

#include <nx/vms/api/data_fwd.h>

#include <nx/utils/uuid.h>

class QString;

namespace ec2 {

struct ApiFullInfoData;

struct ApiUserData;
struct ApiUserRoleData;
struct ApiPredefinedRoleData;
struct ApiDiscoveryData;
struct ApiDiscoverPeerData;
struct ApiConnectionData;
struct ApiSystemIdData;
struct ApiTransactionData;
struct ApiTranLogFilter;
struct ApiDiscoveredServerData;

struct ApiMiscData;
typedef std::vector<ApiMiscData> ApiMiscDataList;

typedef std::vector<ApiTransactionData> ApiTransactionDataList;

typedef std::vector<ApiUserData> ApiUserDataList;
typedef std::vector<ApiUserRoleData> ApiUserRoleDataList;
typedef std::vector<ApiPredefinedRoleData> ApiPredefinedRoleDataList;
typedef std::vector<ApiDiscoveryData> ApiDiscoveryDataList;
typedef std::vector<ApiDiscoveredServerData> ApiDiscoveredServerDataList;

/**
 * Wrapper to be used for overloading as a distinct type for nx::vms::api::StorageData api requests.
 */
struct ParentId
{
    QnUuid id;
    ParentId() = default;
    ParentId(const QnUuid& id): id(id) {}
};

#define QN_EC2_API_DATA_TYPES \
    (ApiFullInfoData)\
    (ApiUserData)\
    (ApiUserRoleData)\
    (ApiPredefinedRoleData)\
    (ApiDiscoveryData)\
    (ApiDiscoverPeerData)\
    (ApiConnectionData)\
    (ApiSystemIdData)\
    (ApiMiscData)\
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
