#include "discovered_peers_rest_handler.h"

#include "utils/network/tcp_connection_priv.h"
#include "utils/network/module_finder.h"
#include "utils/common/model_functions.h"

int QnDiscoveredPeersRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)
    Q_UNUSED(params)

    result.setReply(QnModuleFinder::instance()->foundModules());

    return CODE_OK;
}
