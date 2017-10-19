#include "merge_systems_rest_handler.h"

#include <chrono>

#include <QtCore/QRegExp>

#include <nx/utils/log/log.h>

#include <nx/vms/utils/vms_utils.h>
#include <nx/vms/utils/server_merge_processor.h>

#include <audit/audit_manager.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <network/connection_validator.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

#include "media_server/serverutil.h"
#include "media_server/server_connector.h"

QnMergeSystemsRestHandler::QnMergeSystemsRestHandler(ec2::AbstractTransactionMessageBus* messageBus):
    QnJsonRestHandler(),
    m_messageBus(messageBus)
{}

int QnMergeSystemsRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(MergeSystemData(params)), owner, result);
}

int QnMergeSystemsRestHandler::executePost(
    const QString& path,
    const QnRequestParams& params,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    Q_UNUSED(params);
    MergeSystemData data = QJson::deserialized<MergeSystemData>(body);
    return execute(std::move(data), owner, result);
}

int QnMergeSystemsRestHandler::execute(
    MergeSystemData data,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult &result)
{
    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
        return QnPermissionsHelper::notOwnerError(result);

    nx::vms::utils::SystemMergeProcessor systemMergeProcessor(
        owner->commonModule(),
        getDataDirectory());
    const auto resultCode = systemMergeProcessor.merge(
        owner->accessRights(),
        owner->authSession(),
        data,
        result);
    if (resultCode != nx_http::StatusCode::ok)
        return resultCode;

    QnMediaServerResourcePtr server =
        owner->commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(
            owner->commonModule()->moduleGUID());
    NX_ASSERT(server);
    // TODO: #ak Following call better be made on event "current server data changed".
    nx::ServerSetting::setAuthKey(server->getAuthKey().toUtf8());

    nx::vms::discovery::ModuleEndpoint module(
        systemMergeProcessor.remoteModuleInformation(),
        {QUrl(data.url).host(), (uint16_t) systemMergeProcessor.remoteModuleInformation().port});
    owner->commonModule()->moduleDiscoveryManager()->checkEndpoint(module.endpoint, module.id);

    const auto connectionResult = 
        QnConnectionValidator::validateConnection(
            systemMergeProcessor.remoteModuleInformation());

    /* Connect to server if it is compatible */
    if (connectionResult == Qn::SuccessConnectionResult && QnServerConnector::instance())
        QnServerConnector::instance()->addConnection(module);

    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_SystemmMerge);
    qnAuditManager->addAuditRecord(auditRecord);

    return nx_http::StatusCode::ok;
}
