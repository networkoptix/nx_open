#include "updates2_action_rest_handler.h"
#include <utils/common/app_info.h>
#include <nx/api/updates2/available_update_info_data.h>
#include "detail/update_request_data_factory.h"
#include "detail/update_info_helper.h"


namespace nx {
namespace mediaserver {
namespace rest {
namespace updates2 {

JsonRestResponse Updates2ActionRestHandler::executeGet(const JsonRestRequest& request)
{
    JsonRestResponse();
}

JsonRestResponse Updates2ActionRestHandler::executeDelete(const JsonRestRequest& request)
{
    return JsonRestResponse();
}

JsonRestResponse Updates2ActionRestHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    return JsonRestResponse();
}

JsonRestResponse Updates2ActionRestHandler::executePut(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    return JsonRestResponse();
}

} // namespace updates2
} // namespace rest
} // namespace mediaserver
} // namespace nx
