#include "utils.h"

namespace nx::cloud::storage::client::aws_s3 {

ResultCode toResultCode(aws::Result result)
{
    switch (result.code())
    {
        case aws::ResultCode::ok:
            return ResultCode::ok;

        case aws::ResultCode::unauthorized:
            return ResultCode::unauthorized;

        default:
            return ResultCode::ioError;
    }
}

} // namespace nx::cloud::storage::client::aws_s3
