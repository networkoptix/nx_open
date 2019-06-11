#include "retry_update.h"

#include <nx/vms/server/server_update_manager.h>
#include <media_server/media_server_module.h>

#include "update_status_rest_handler.h"

namespace nx::vms::server::rest::handlers {

RetryUpdate::RetryUpdate(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

int RetryUpdate::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const QByteArray& /*body*/,
    const QByteArray& /*srcBodyContentType*/,
    QByteArray& result,
    QByteArray& resultContentType,
    const QnRestConnectionProcessor* processor)
{
    // Hidden feature, mainly for testing purposes. This parameter should not appear in the
    // documentation.
    const bool forceRedownload = params.value("force") == "true";
    NX_VERBOSE(this, "executePost(), force=%1", forceRedownload);
    serverModule()->updateManager()->retry(forceRedownload);

    QnUpdateStatusRestHandler handler(serverModule());
    return handler.executeGet(path, {}, result, resultContentType, processor);
}

} // namespace nx::vms::server::rest::handlers
