#pragma once

#include <nx/cloud/storage/client/result_code.h>
#include <nx/cloud/aws/api_types.h>

namespace nx::cloud::storage::client::aws_s3 {

ResultCode toResultCode(aws::Result result);

} // namespace nx::cloud::storage::client::aws_s3
