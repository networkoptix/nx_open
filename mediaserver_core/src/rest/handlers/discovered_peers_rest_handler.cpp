#include "discovered_peers_rest_handler.h"

#include <common/common_module.h>
#include <network/tcp_connection_priv.h>
#include <nx_ec/data/api_discovery_data.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/discovery/manager.h>
#include <rest/server/rest_connection_processor.h>

int QnDiscoveredPeersRestHandler::executeGet(
    const QString &path,
    const QnRequestParams &params,
    QnJsonRestResult &result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path)
    Q_UNUSED(params)

    result.setReply(ec2::getServers(owner->commonModule()->moduleDiscoveryManager()));
    return CODE_OK;
}
