#include "runtime_info_rest_handler.h"

#include <api/runtime_info_manager.h>

#include <common/common_module.h>

#include <rest/server/rest_connection_processor.h>

#include <nx/network/http/http_types.h>

int QnRuntimeInfoRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    auto runtimeInfoManager = owner->commonModule()->runtimeInfoManager();

    std::vector<ec2::ApiRuntimeData> items;
    if (params.contains("local"))
    {
        items.push_back(runtimeInfoManager->localInfo().data);
    }
    else
    {
        for (const QnPeerRuntimeInfo& item: runtimeInfoManager->items()->getItems())
            items.push_back(item.data);
    }

    result.setReply(items);
    return nx::network::http::StatusCode::ok;
}
