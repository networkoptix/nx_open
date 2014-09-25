#include "discovered_peers_rest_handler.h"

#include "utils/network/tcp_connection_priv.h"
#include "utils/network/module_finder.h"
#include "utils/common/model_functions_fwd.h"
#include "utils/common/model_functions.h"

namespace {
    struct DiscoveredPeersResult {
        QList<QnModuleInformation> modules;
    };
    QN_FUSION_DECLARE_FUNCTIONS(DiscoveredPeersResult, (json))
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DiscoveredPeersResult, (json), (modules))
}

int QnDiscoveredPeersRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    Q_UNUSED(path)
    Q_UNUSED(params)

    DiscoveredPeersResult jsonResult;
    jsonResult.modules = QnModuleFinder::instance()->foundModules();

    result.setReply(jsonResult);

    return CODE_OK;
}
