#pragma once

#include <nx/network/http/http_types.h>

#include "vms_utils.h"

struct QnJsonRestResult;
class QnCommonModule;

namespace nx::vms::cloud_integration { class CloudConnectionManager; }

namespace nx::vms::utils {

class DetachServerProcessor
{
public:
    DetachServerProcessor(
        QnCommonModule* commonModule,
        nx::vms::cloud_integration::CloudConnectionManager* cloudConnectionManager);

    nx::network::http::StatusCode::Value detachServer(
        QnJsonRestResult* result);

private:
    QnCommonModule* m_commonModule;
    nx::vms::cloud_integration::CloudConnectionManager* m_cloudConnectionManager;
};

} // namespace nx::vms::utils
