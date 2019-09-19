#pragma once

#include <nx/sql/types.h>
#include <nx/cloud/storage/service/api/result.h>

#include <nx/cloud/aws/api_types.h>

namespace nx::cloud::storage::service::controller::utils {

static constexpr char kDefaultBucketRegion[] = "us-east-1";

api::ResultCode toResultCode(nx::sql::DBResult dbResult);

api::ResultCode toResultCode(aws::ResultCode resultCode);

api::Result toResult(const aws::Result& result);

} // namespace nx::cloud::storage::service::controller::utils
