#include "update_information_rest_handler2.h"
#include <media_server/media_server_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/mediaserver/settings.h>
#include <nx/update/update_information.h>
#include <utils/common/request_param.h>

namespace nx {

static const QString kPublicationKeyParamName = "publicationKey";
static const QString kNoPropagationParamName = "noPropagation";

namespace {

static JsonRestResponse checkUpdateServerForUpdates(const QString& publicationKey)
{
    return JsonRestResponse();
}

static JsonRestResponse checkOtherServerForUpdates(const QString& publicationKey)
{
    return JsonRestResponse();
}

static JsonRestResponse getUpdateInformationFromGlobalSettings()
{
    return JsonRestResponse();
}

} // namespace

JsonRestResponse UpdateInformationRestHandler::executeGet(const JsonRestRequest& request)
{
    auto mediaServer = qnServerModule->resourcePool()->getResourceById<QnMediaServerResource>(
        qnServerModule->commonModule()->moduleGUID());

    NX_CRITICAL(mediaServer);
    if (request.params.contains(kPublicationKeyParamName))
    {
        // #TODO #akulikov Check for the ini config flag for forcing update check on this server.
        if (mediaServer->getServerFlags().testFlag(vms::api::SF_HasPublicIP))
            return checkUpdateServerForUpdates(request.params[kPublicationKeyParamName]);

        if (!request.params.contains(kNoPropagationParamName))
            return checkOtherServerForUpdates(request.params[kPublicationKeyParamName]);
    }

    return getUpdateInformationFromGlobalSettings();
}

} // namespace nx
