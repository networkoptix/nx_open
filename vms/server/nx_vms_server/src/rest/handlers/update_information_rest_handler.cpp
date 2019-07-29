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
#include <nx/update/update_check.h>
#include <media_server/media_server_module.h>

#include <nx/vms/server/settings.h>
#include <nx/system_commands.h>

namespace {

static const QString kVersionParamName = "version";

using MaybeUpdateInfoCategory = std::optional<nx::CommonUpdateManager::InformationCategory>;

static MaybeUpdateInfoCategory categoryFromString(const QString& requestedVersion)
{
    if (requestedVersion.isNull() || requestedVersion == "target")
        return nx::CommonUpdateManager::InformationCategory::target;

    if (requestedVersion == "installed")
        return nx::CommonUpdateManager::InformationCategory::installed;

    return std::nullopt;
}

static bool updateInformationShouldBeObtainedLocally(const QString& requestedVersion)
{
    return categoryFromString(requestedVersion).has_value();
}

} // namespace

struct UpdateInformationRequestData: public QnMultiserverRequestData
{
    UpdateInformationRequestData() = default;

    virtual void loadFromParams(
        QnResourcePool* resourcePool,
        const QnRequestParamList& params) override
    {
        QnMultiserverRequestData::loadFromParams(resourcePool, params);
        if (params.contains(kVersionParamName))
            version = params.value(kVersionParamName);
    }

    virtual QnRequestParamList toParams() const override
    {
        auto result = QnMultiserverRequestData::toParams();
        if (!version.isNull())
            result.insert(kVersionParamName, version);
        return result;
    }

    static UpdateInformationRequestData create(
        const QString& path,
        const QnRequestParamList& params,
        const QnRestConnectionProcessor* processor)
    {
        auto request = QnMultiserverRequestData::fromParams<UpdateInformationRequestData>(
            processor->resourcePool(),
            params);

        request.path = path;
        request.port = processor->owner()->getPort();

        return request;
    }

    QString path;
    int port = -1;
    QString version;
};

qint64 QnUpdateInformationRestHandler::freeSpaceForUpdate() const
{
    auto updatesDir = serverModule()->settings().dataDir();
    if (updatesDir.isEmpty())
        updatesDir = QDir::tempPath();

    return nx::SystemCommands().freeSpace(updatesDir.toStdString());
}

void loadFreeSpaceRemotely(
    const UpdateInformationRequestData& request,
    QnCommonModule* commonModule,
    QnUpdateFreeSpaceReply* outReply)
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

    QnMultiserverRequestContext<UpdateInformationRequestData> context(request, request.port);
    detail::requestRemotePeers(commonModule, request.path, *outReply, &context, mergeFunction);
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

int QnUpdateInformationRestHandler::checkInternetForUpdate(
    const UpdateInformationRequestData& request,
    QByteArray* result,
    QByteArray* contentType) const
{
    NX_DEBUG(this, "Checking the Internet for the update information...");
    nx::update::InformationError error;
    auto information = nx::update::updateInformation(
        serverModule()->settings().checkForUpdateUrl(),
        serverModule()->commonModule()->engineVersion(),
        request.version,
        &error);

    NX_DEBUG(
        this, "Checking the Internet for the update information. Done. Success: %1",
        error == nx::update::InformationError::noError);

    if (error == nx::update::InformationError::noError)
    {
        QnFusionRestHandlerDetail::serializeJsonRestReply(
            information,
            request.extraFormatting,
            *result,
            *contentType);
        return nx::network::http::StatusCode::ok;
    }

    using namespace nx::update;
    return QnFusionRestHandler::makeError(
        nx::network::http::StatusCode::ok, toString(error),
        result, contentType, request.format, request.extraFormatting,
        QnRestResult::CantProcessRequest);
}

QnUpdateInformationRestHandler::QnUpdateInformationRestHandler(QnMediaServerModule *serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QnUpdateInformationRestHandler::HandlerType QnUpdateInformationRestHandler::createHandler(
    const UpdateInformationRequestData& request) const
{
    using namespace std::placeholders;
    auto fromMemFn =
        [this, &request](auto memFn)
        {
            return [this, &request, memFn](auto body, auto contenType)
                { return (this->*memFn)(request, body, contenType); };
        };

    if (request.path.endsWith("/freeSpaceForUpdateFiles"))
        return fromMemFn(&QnUpdateInformationRestHandler::handleFreeSpace);

    if (request.path.endsWith("/checkCloudHost"))
        return fromMemFn(&QnUpdateInformationRestHandler::handleCheckCloudHost);

    return fromMemFn(&QnUpdateInformationRestHandler::handleVersion);
}

int QnUpdateInformationRestHandler::handleFreeSpace(
    const UpdateInformationRequestData& request,
    QByteArray* outBody,
    QByteArray* outContentType) const
{
    QnUpdateFreeSpaceReply reply;
    reply.freeSpaceByServerId[serverModule()->commonModule()->moduleGUID()] = freeSpaceForUpdate();
    if (!request.isLocal)
        loadFreeSpaceRemotely(request, serverModule()->commonModule(), &reply);

    QnFusionRestHandlerDetail::serialize(reply, *outBody, *outContentType, request.format);
    return nx::network::http::StatusCode::ok;
}

int QnUpdateInformationRestHandler::handleCheckCloudHost(
    const UpdateInformationRequestData& request,
    QByteArray* outBody,
    QByteArray* outContentType) const
{
    QnCloudHostCheckReply reply;
    reply.cloudHost = serverModule()->globalSettings()->cloudHost();

    if (!request.isLocal)
    {
        QnMultiserverRequestContext<UpdateInformationRequestData> context(request, request.port);
        checkCloudHostRemotely(serverModule()->commonModule(), request.path, reply, &context);
    }

    QnFusionRestHandlerDetail::serialize(reply, *outBody, *outContentType, request.format);
    return nx::network::http::StatusCode::ok;
}

int QnUpdateInformationRestHandler::handleVersion(
    const UpdateInformationRequestData& request,
    QByteArray* outBody,
    QByteArray* outContentType) const
{
    NX_DEBUG(
        this,
        "Received /ec2/updateInformation?version=%1. IsLocal = %2",
        request.version,
        request.isLocal);

    if (updateInformationShouldBeObtainedLocally(request.version))
        return makeUpdateInformationResponseFromLocalData(request, outBody, outContentType);

    return queryUpdateInformationAndMakeResponse(request, outBody, outContentType);
}

int QnUpdateInformationRestHandler::makeUpdateInformationResponseFromLocalData(
    const UpdateInformationRequestData& request,
    QByteArray* result,
    QByteArray* contentType) const
{
    try
    {
        const auto updateInfo = serverModule()->updateManager()->updateInformation(
            *categoryFromString(request.version));

        QnFusionRestHandlerDetail::serializeJsonRestReply(
            updateInfo,
            request.extraFormatting,
            *result,
            *contentType);

        return nx::network::http::StatusCode::ok;
    }
    catch (const std::exception& e)
    {
        NX_DEBUG(this, e.what());
        return QnFusionRestHandler::makeError(
            nx::network::http::StatusCode::ok,
            toString(nx::update::InformationError::noNewVersion),
            result,
            contentType,
            request.format,
            request.extraFormatting,
            QnRestResult::CantProcessRequest);
    }
}

int QnUpdateInformationRestHandler::queryUpdateInformationAndMakeResponse(
    const UpdateInformationRequestData& request,
    QByteArray* result,
    QByteArray* contentType) const
{
    if (!request.isLocal)
        return checkForUpdateInformationRemotely(request, result, contentType);

    if (serverHasInternet(serverModule()->commonModule()->moduleGUID()))
        return checkInternetForUpdate(request, result, contentType);

    return QnFusionRestHandler::makeError(
        nx::network::http::StatusCode::ok,
        toString(QnRestResult::Error::CantProcessRequest),
        result,
        contentType,
        request.format,
        request.extraFormatting,
        QnRestResult::CantProcessRequest);
}

bool QnUpdateInformationRestHandler::serverHasInternet(const QnUuid& serverId) const
{
    const auto resource =
        serverModule()->commonModule()->resourcePool()->getResourceById(serverId);

    const auto mediaServer = resource.dynamicCast<QnMediaServerResource>();
    return mediaServer && mediaServer->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP);
}

int QnUpdateInformationRestHandler::checkForUpdateInformationRemotely(
    const UpdateInformationRequestData& request,
    QByteArray* result,
    QByteArray* contentType) const
{
    const auto mergeFunction =
        [](
            const QnUuid& /*serverId*/,
            bool success,
            const QnJsonRestResult& reply,
            QnJsonRestResult& outputReply)
        {
            if (outputReply.deserialized<nx::update::Information>().isValid())
                return;

            if (success)
                outputReply = reply;
        };

    const auto ifParticipantPredicate =
        [this](const QnUuid& serverId, const nx::vms::api::SoftwareVersion&)
        {
            return serverHasInternet(serverId)
                ? detail::ParticipationStatus::participant
                : detail::ParticipationStatus::notInList;
        };

    QnJsonRestResult outputReply;
    outputReply.error = QnRestResult::Error::CantProcessRequest;
    outputReply.errorString = toString(nx::update::InformationError::networkError);

    QnMultiserverRequestContext<UpdateInformationRequestData> context(request, request.port);
    detail::requestRemotePeers(
        serverModule()->commonModule(),
        request.path,
        outputReply,
        &context,
        mergeFunction,
        ifParticipantPredicate);

    if (outputReply.deserialized<nx::update::Information>().isValid())
    {
        QnFusionRestHandlerDetail::serialize(
            outputReply,
            *result,
            *contentType,
            request.format,
            request.extraFormatting);

        return nx::network::http::StatusCode::ok;
    }

    return QnFusionRestHandler::makeError(
        nx::network::http::StatusCode::ok,
        outputReply.errorString,
        result,
        contentType,
        request.format,
        request.extraFormatting,
        QnRestResult::CantProcessRequest);
}

int QnUpdateInformationRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    const auto request = UpdateInformationRequestData::create(path, params, processor);
    const auto requestHandler = createHandler(request);
    return requestHandler(&result, &contentType);
}
