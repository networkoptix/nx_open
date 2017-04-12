#include "discovered_peers_rest_handler.h"

#include "network/tcp_connection_priv.h"
#include "network/module_finder.h"
#include "nx/fusion/model_functions.h"
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

int QnDiscoveredPeersRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    result.setReply(owner->commonModule()->moduleFinder()->discoveredServers());

    return CODE_OK;
}
