#pragma once

#include <nx/vms/api/data_fwd.h>

#include <nx/utils/uuid.h>

class QString;

namespace ec2 {

struct ApiConnectionData;
struct ApiTransactionData;
struct ApiTranLogFilter;
struct ApiDiscoveredServerData;
struct ApiMiscData;

typedef std::vector<ApiMiscData> ApiMiscDataList;
typedef std::vector<ApiTransactionData> ApiTransactionDataList;
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
    (ApiConnectionData)\
    (ApiMiscData)\
    (ApiDiscoveredServerData)\

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    QN_EC2_API_DATA_TYPES,
    (ubjson)(xml)(json)(sql_record)(csv_record)
);

} // namespace ec2
