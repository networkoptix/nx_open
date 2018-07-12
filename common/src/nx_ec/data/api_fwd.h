#pragma once

#include <nx/vms/api/data_fwd.h>

#include <nx/utils/uuid.h>

class QString;

namespace ec2 {

struct ApiTransactionData;
struct ApiTranLogFilter;

typedef std::vector<ApiTransactionData> ApiTransactionDataList;

} // namespace ec2
