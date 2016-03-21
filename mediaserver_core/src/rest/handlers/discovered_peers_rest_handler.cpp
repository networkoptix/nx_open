#include "discovered_peers_rest_handler.h"

#include "network/tcp_connection_priv.h"
#include "network/module_finder.h"
#include "utils/common/model_functions.h"

int QnDiscoveredPeersRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    result.setReply(QnModuleFinder::instance()->discoveredServers());

    return CODE_OK;
}
