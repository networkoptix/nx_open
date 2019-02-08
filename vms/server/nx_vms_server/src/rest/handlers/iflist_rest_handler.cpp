#include "iflist_rest_handler.h"

#include <network/tcp_connection_priv.h>

#include "network_address_entry.h"

int QnIfListRestHandler::executeGet(
    const QString& /*path*/, const QnRequestParams& /*params*/,
    QnJsonRestResult& result, const QnRestConnectionProcessor* /*processor*/)
{
    result.setReply(systemNetworkAddressEntryList());
    return nx::network::http::StatusCode::ok;
}
