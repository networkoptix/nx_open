#include "detach_server_processor.h"

#include <nx/vms/cloud_integration/cloud_connection_manager.h>

#include <nx/utils/log/log.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <rest/server/json_rest_result.h>

namespace nx::vms::utils {

DetachServerProcessor::DetachServerProcessor(
    QnCommonModule* commonModule,
    nx::vms::cloud_integration::CloudConnectionManager* cloudConnectionManager)
    :
    m_commonModule(commonModule),
    m_cloudConnectionManager(cloudConnectionManager)
{
}

nx::network::http::StatusCode::Value DetachServerProcessor::detachServer(
    QnJsonRestResult* result)
{
    QString errStr;
    if (nx::vms::utils::resetSystemToStateNew(m_commonModule))
    {
        if (!m_cloudConnectionManager->detachSystemFromCloud())
        {
            errStr = lm("Cannot detach from cloud. Failed to reset cloud attributes. cloudSystemId %1")
                .arg(m_commonModule->globalSettings()->cloudSystemId());
        }
    }
    else
    {
        errStr = lm("Cannot detach from system. Failed to reset system to state new.");
    }

    if (errStr.isEmpty())
    {
        NX_DEBUG(this, lm("Detaching server from system finished."));
    }
    else
    {
        NX_WARNING(this, errStr);
        result->setError(QnJsonRestResult::CantProcessRequest, errStr);
    }

    return nx::network::http::StatusCode::ok;
}

} // namespace nx::vms::utils
