#include "updates2_info_rest_handler.h"
#include <utils/common/app_info.h>
#include <nx/api/updates2/available_update_info_data.h>
#include "detail/update_request_data_factory.h"
#include "detail/update_info_helper.h"


namespace nx {
namespace mediaserver {
namespace rest {
namespace updates2 {

namespace {

JsonRestResponse createResponse()
{
    JsonRestResponse result(nx_http::StatusCode::ok);
    result.json.setReply(api::AvailableUpdatesInfo());

    QnSoftwareVersion version;
    if (!detail::findUpdateInfo(&version))
        return result;

    result.json.setReply(api::AvailableUpdatesInfo(version.toString()));
    return result;
}
} // namespace

JsonRestResponse Updates2InfoRestHandler::executeGet(const JsonRestRequest& request)
{
    NX_ASSERT(request.path == kUpdates2AvailablePath);
    if (request.path != kUpdates2AvailablePath)
        return nx_http::StatusCode::notFound;

    return createResponse();
}

JsonRestResponse Updates2InfoRestHandler::executeDelete(const JsonRestRequest& request)
{
    return JsonRestResponse();
}

JsonRestResponse Updates2InfoRestHandler::executePost(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    return JsonRestResponse();
}

JsonRestResponse Updates2InfoRestHandler::executePut(
    const JsonRestRequest& request,
    const QByteArray& body)
{
    return JsonRestResponse();
}

} // namespace updates2
} // namespace rest
} // namespace mediaserver
} // namespace nx
