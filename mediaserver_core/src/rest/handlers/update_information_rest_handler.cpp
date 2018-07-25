#include "update_information_rest_handler.h"
#include "private/multiserver_request_helper.h"
#include <QtCore/QDir>
#include <utils/common/util.h>
#include <media_server/settings.h>
#include <api/model/update_information_reply.h>
#include <api/helpers/empty_request_data.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/tcp_listener.h>
#include <nx/vms/discovery/manager.h>
#include <api/global_settings.h>
#include <media_server/media_server_module.h>
#include <nx/vms/api/types/resource_types.h>
#include <nx/update/update_information.h>
#include <media_server/media_server_module.h>
#include <nx/mediaserver/settings.h>

namespace {

static const QString kPublicationKeyParamName = "publicationKey";

struct UpdateInformationRequestData: public QnMultiserverRequestData
{
    UpdateInformationRequestData() = default;

    virtual void loadFromParams(
        QnResourcePool* resourcePool,
        const QnRequestParamList& params) override
    {
        QnMultiserverRequestData::loadFromParams(resourcePool, params);
        if (params.contains(kPublicationKeyParamName))
            publicationId = params.value(kPublicationKeyParamName);
    }

    virtual QnRequestParamList toParams() const override
    {
        auto result = QnMultiserverRequestData::toParams();
        if (!publicationId.isNull())
            result.insert(kPublicationKeyParamName, publicationId);
        return result;
    }

    QString publicationId;
};

qint64 freeSpaceForUpdate()
{
    auto updatesDir = qnServerModule->settings().dataDir();
    if (updatesDir.isEmpty())
        updatesDir = QDir::tempPath();

    return getDiskFreeSpace(updatesDir);
}

void loadFreeSpaceRemotely(
    QnCommonModule* commonModule,
    const QString& path,
    QnUpdateFreeSpaceReply& outputReply,
    QnMultiserverRequestContext<UpdateInformationRequestData>* context)
{
    auto mergeFunction =
        [](
            const QnUuid& serverId,
            bool success,
            const QnUpdateFreeSpaceReply& reply,
            QnUpdateFreeSpaceReply& outputReply)
        {
            if (success)
            {
                const auto freeSpace = reply.freeSpaceByServerId.value(serverId, -1);
                if (freeSpace > 0)
                    outputReply.freeSpaceByServerId[serverId] = freeSpace;
            }
        };

    detail::requestRemotePeers(commonModule, path, outputReply, context, mergeFunction);
}

void checkCloudHostRemotely(
    QnCommonModule* commonModule,
    const QString& path,
    QnCloudHostCheckReply& outputReply,
    QnMultiserverRequestContext<UpdateInformationRequestData>* context)
{
    auto mergeFunction =
        [](
            const QnUuid& serverId,
            bool success,
            const QnCloudHostCheckReply& reply,
            QnCloudHostCheckReply& outputReply)
        {
            if (success && !reply.cloudHost.isEmpty() && reply.cloudHost != outputReply.cloudHost)
                outputReply.failedServers.append(serverId);
        };

    detail::requestRemotePeers(commonModule, path, outputReply, context, mergeFunction);
}

static int checkInternetForUpdate(
    const QString& publicationKey,
    QByteArray* result,
    QByteArray* contentType,
    const UpdateInformationRequestData& request)
{
    nx::update::InformationError error;
    auto information = nx::update::updateInformation(
        qnServerModule->settings().checkForUpdateUrl(),
        publicationKey,
        &error);

    if (error == nx::update::InformationError::noError)
    {
        QnFusionRestHandlerDetail::serialize(information, *result, *contentType, request.format);
        return nx::network::http::StatusCode::ok;
    }

    using namespace nx::update;
    return QnFusionRestHandler::makeError(
        nx::network::http::StatusCode::ok, toString(error),
        result, contentType, request.format, request.extraFormatting,
        QnRestResult::CantProcessRequest);
}

static int checkForUpdateInformationRemotely(
    QnCommonModule* commonModule,
    const QString& path,
    QByteArray* result,
    QByteArray* contentType,
    QnMultiserverRequestContext<UpdateInformationRequestData>* context)
{
    bool done = false;
    auto mergeFunction =
        [&done](
            const QnUuid& serverId,
            bool success,
            const nx::update::Information& reply,
            nx::update::Information& outputReply)
        {
            if (done)
                return;

            if (success && reply.isValid() && !done)
            {
                outputReply = reply;
                done = true;
            }
        };

    nx::update::Information outputReply;
    detail::requestRemotePeers(commonModule, path, outputReply, context, mergeFunction);

    if (done)
    {
        QnFusionRestHandlerDetail::serialize(outputReply, *result, *contentType, context->request().format);
        return nx::network::http::StatusCode::ok;
    }

    using namespace nx::update;
    return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
        toString(InformationError::notFoundError),
        result, contentType, context->request().format, context->request().extraFormatting,
        QnRestResult::CantProcessRequest);
}

static int getUpdateInformationFromGlobalSettings(
    QByteArray* result,
    QByteArray* contentType,
    const UpdateInformationRequestData& request)
{
    *contentType = Qn::serializationFormatToHttpContentType(request.format);
    *result = qnServerModule->commonModule()->globalSettings()->updateInformation();
    if (result->isEmpty())
        *result = "{}";

    return nx::network::http::StatusCode::ok;
}

} // namespace

int QnUpdateInformationRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    const auto request = QnMultiserverRequestData::fromParams<UpdateInformationRequestData>(
        processor->resourcePool(),
        params);

    QnMultiserverRequestContext<UpdateInformationRequestData> context(
        request,
        processor->owner()->getPort());

    if (path.endsWith(lit("/freeSpaceForUpdateFiles")))
    {
        QnUpdateFreeSpaceReply reply;
        const auto moduleGuid = processor->commonModule()->moduleGUID();
        reply.freeSpaceByServerId[moduleGuid] = freeSpaceForUpdate();

        if (!request.isLocal)
            loadFreeSpaceRemotely(processor->commonModule(), path, reply, &context);

        QnFusionRestHandlerDetail::serialize(reply, result, contentType, request.format);
        return nx::network::http::StatusCode::ok;
    }
    else if (path.endsWith(lit("/checkCloudHost")))
    {
        QnCloudHostCheckReply reply;
        reply.cloudHost = processor->globalSettings()->cloudHost();

        if (!request.isLocal)
            checkCloudHostRemotely(processor->commonModule(), path, reply, &context);

        QnFusionRestHandlerDetail::serialize(reply, result, contentType, request.format);
        return nx::network::http::StatusCode::ok;
    }

    auto mediaServer = qnServerModule->resourcePool()->getResourceById<QnMediaServerResource>(
        qnServerModule->commonModule()->moduleGUID());

    NX_CRITICAL(mediaServer);
    if (params.contains(kPublicationKeyParamName))
    {
        if (mediaServer->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP) || request.isLocal)
        {
            return checkInternetForUpdate(
                params.value(kPublicationKeyParamName), &result, &contentType, request);
        }

        return checkForUpdateInformationRemotely(
            processor->commonModule(), path, &result, &contentType, &context);
    }

    return getUpdateInformationFromGlobalSettings(&result, &contentType, request);
}
