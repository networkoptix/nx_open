#pragma once

#include <nx/vms/api/data_fwd.h>

#include <nx/utils/uuid.h>

class QString;

namespace ec2 {

struct ApiTransactionData;
struct ApiTranLogFilter;

typedef std::vector<ApiTransactionData> ApiTransactionDataList;

/**
 * Wrapper to be used for overloading as a distinct type for nx::vms::api::StorageData api requests.
 */
struct ParentId
{
    QnUuid id;
    ParentId() = default;
    ParentId(const QnUuid& id): id(id) {}
};

} // namespace ec2
