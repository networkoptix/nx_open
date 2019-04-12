#include "rebuild_archive_rest_handler.h"
#include "network/tcp_connection_priv.h"
#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include <media_server/serverutil.h>
#include "qcoreapplication.h"
#include <media_server/settings.h>
#include "recorder/device_file_catalog.h"
#include "recorder/storage_manager.h"
#include "core/resource/storage_resource.h"
#include <nx/network/rest/nx_network_rest_ini.h>

namespace nx::vms::server {

RebuildArchiveRestHandler::RebuildArchiveRestHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

nx::network::rest::Response RebuildArchiveRestHandler::executeGet(
    const nx::network::rest::Request& request)
{
    if (!nx::network::rest::ini().allowGetModifications && request.param("action"))
        return nx::network::rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

nx::network::rest::Response RebuildArchiveRestHandler::executePost(
    const nx::network::rest::Request& request)
{
    const auto method = request.paramOrDefault("action");
    const auto mainPool = request.param("mainPool");
    const auto storagePool = (!mainPool || mainPool->toInt() != 0)
        ? serverModule()->normalStorageManager()
        : serverModule()->backupStorageManager();

    auto reply = storagePool->rebuildInfo();
    if (method == "start")
        reply = storagePool->rebuildCatalogAsync();
    else if (method == "stop")
        storagePool->cancelRebuildCatalogAsync();

    return nx::network::rest::Response::reply(reply);
}

} // namespace nx::vms::server
