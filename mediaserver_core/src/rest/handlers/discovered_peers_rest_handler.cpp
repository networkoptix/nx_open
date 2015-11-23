#include "discovered_peers_rest_handler.h"

#include "network/tcp_connection_priv.h"
#include "network/module_finder.h"
#include "utils/common/model_functions.h"

int QnDiscoveredPeersRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    Q_UNUSED(path)

    bool showAddresses = params.value(lit("showAddresses"), lit("false")) != lit("false");

    if (showAddresses)
        result.setReply(QnModuleFinder::instance()->foundModulesWithAddresses());
    else
        result.setReply(QnModuleFinder::instance()->foundModules());

    return CODE_OK;
}
