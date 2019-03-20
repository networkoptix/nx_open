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
}

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

    QString version;
};

qint64 QnUpdateInformationRestHandler::freeSpaceForUpdate() const
{
    auto updatesDir = m_settings->dataDir();
    if (updatesDir.isEmpty())
        updatesDir = QDir::tempPath();

    return nx::SystemCommands().freeSpace(updatesDir.toStdString());
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

int QnUpdateInformationRestHandler::checkInternetForUpdate(
    const QnRequestParamList& params,
    const QString& publicationKey,
    QByteArray* result,
    QByteArray* contentType,
    const UpdateInformationRequestData& request) const
{
    nx::update::InformationError error;
    auto information = nx::update::updateInformation(m_settings->checkForUpdateUrl(),
        m_engineVersion, publicationKey, &error);

    if (error == nx::update::InformationError::noError)
    {
        QnFusionRestHandlerDetail::serializeJsonRestReply(information, params, *result, *contentType,
            QnRestResult());
        return nx::network::http::StatusCode::ok;
    }

    using namespace nx::update;
    return QnFusionRestHandler::makeError(
        nx::network::http::StatusCode::ok, toString(error),
        result, contentType, request.format, request.extraFormatting,
        QnRestResult::CantProcessRequest);
}

static int checkForUpdateInformationRemotely(
    const QnRequestParamList& params,
    QnCommonModule* commonModule,
    const QString& path,
    QByteArray* result,
    QByteArray* contentType,
    QnMultiserverRequestContext<UpdateInformationRequestData>* context)
{
    bool done = false;
    auto mergeFunction =
        [&done](
            const QnUuid& /*serverId*/, bool success, const QnJsonRestResult& reply,
            QnJsonRestResult& outputReply)
        {
            nx::update::Information updateInfo = reply.deserialized<nx::update::Information>();
            if (done)
                return;

            if (success && updateInfo.isValid() && !done)
            {
                outputReply = reply;
                done = true;
            }
        };

    QnJsonRestResult outputReply;
    detail::requestRemotePeers(commonModule, path, outputReply, context, mergeFunction);

    if (done)
    {
        QnFusionRestHandlerDetail::serialize(
            outputReply, *result, *contentType, Qn::JsonFormat,
            params.contains("extraFormatting"));
        return nx::network::http::StatusCode::ok;
    }

    using namespace nx::update;
    return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
        toString(InformationError::notFoundError),
        result, contentType, context->request().format, context->request().extraFormatting,
        QnRestResult::CantProcessRequest);
}

static int makeUpdateInformationResponse(
    const QnRequestParamList& params,
    const QByteArray& updateInformationData,
    QByteArray* result,
    QByteArray* contentType,
    const UpdateInformationRequestData& request)
{
    nx::update::Information resultUpdateInformation;
    const auto deserializeResult = nx::update::fromByteArray(updateInformationData,
        &resultUpdateInformation, nullptr);

    if (deserializeResult != nx::update::FindPackageResult::ok || !resultUpdateInformation.isValid())
    {
        return QnFusionRestHandler::makeError(nx::network::http::StatusCode::ok,
            toString(nx::update::InformationError::notFoundError), result, contentType,
            request.format, request.extraFormatting, QnRestResult::CantProcessRequest);
    }

    QnFusionRestHandlerDetail::serializeJsonRestReply(resultUpdateInformation, params, *result,
        *contentType, QnRestResult());

    return nx::network::http::StatusCode::ok;
}

QnUpdateInformationRestHandler::QnUpdateInformationRestHandler(
    const nx::vms::server::Settings* settings,
    const nx::vms::api::SoftwareVersion& engineVersion)
    :
    m_settings(settings),
    m_engineVersion(engineVersion)
{
}

int QnUpdateInformationRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    auto commonModule = processor->commonModule();
    const auto request = QnMultiserverRequestData::fromParams<UpdateInformationRequestData>(
        processor->resourcePool(), params);

    QnMultiserverRequestContext<UpdateInformationRequestData> context(
        request, processor->owner()->getPort());

    if (path.endsWith(lit("/freeSpaceForUpdateFiles")))
    {
        QnUpdateFreeSpaceReply reply;
        const auto moduleGuid = commonModule->moduleGUID();
        reply.freeSpaceByServerId[moduleGuid] = freeSpaceForUpdate();

        if (!request.isLocal)
            loadFreeSpaceRemotely(commonModule, path, reply, &context);

        QnFusionRestHandlerDetail::serialize(reply, result, contentType, request.format);
        return nx::network::http::StatusCode::ok;
    }

    if (path.endsWith(lit("/checkCloudHost")))
    {
        QnCloudHostCheckReply reply;
        reply.cloudHost = processor->globalSettings()->cloudHost();

        if (!request.isLocal)
            checkCloudHostRemotely(commonModule, path, reply, &context);

        QnFusionRestHandlerDetail::serialize(reply, result, contentType, request.format);
        return nx::network::http::StatusCode::ok;
    }

    auto mediaServer = commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
        commonModule->moduleGUID());

    NX_CRITICAL(mediaServer);
    const auto versionValue = params.value(kVersionParamName);

    if (versionValue.isNull() || versionValue == "target")
    {
        return makeUpdateInformationResponse(params,
            commonModule->globalSettings()->targetUpdateInformation(), &result, &contentType,
            request);
    }

    if (versionValue == "installed")
    {
        return makeUpdateInformationResponse(params,
            commonModule->globalSettings()->installedUpdateInformation(), &result, &contentType,
            request);
    }

    if (mediaServer->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP) || request.isLocal)
        return checkInternetForUpdate(params, versionValue, &result, &contentType, request);

    return checkForUpdateInformationRemotely(params, commonModule, path, &result, &contentType,
        &context);
}
