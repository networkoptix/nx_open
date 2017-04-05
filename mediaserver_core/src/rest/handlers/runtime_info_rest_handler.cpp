#include "runtime_info_rest_handler.h"
#include <api/runtime_info_manager.h>
#include <utils/network/http/httptypes.h>

int QnRuntimeInfoRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor*)
{
    std::vector<ec2::ApiRuntimeData> items;
    if (params.contains("local"))
    {
        items.push_back(QnRuntimeInfoManager::instance()->localInfo().data);
    }
    else
    {
        for (const QnPeerRuntimeInfo& item: QnRuntimeInfoManager::instance()->items()->getItems())
            items.push_back(item.data);
    }

    result.setReply(items);
    return nx_http::StatusCode::ok;
}
