#include "system_settings_handler.h"

#include <api/global_settings.h>
#include <api/model/system_settings_reply.h>
#include <api/resource_property_adaptor.h>
#include <audit/audit_manager.h>
#include <common/common_module.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>
#include <nx/network/rest/nx_network_rest_ini.h>
#include <rest/server/rest_connection_processor.h>

namespace nx::vms::server {

SystemSettingsProcessor::SystemSettingsProcessor(
    QnCommonModule* commonModule,
    const QnAuthSession& authSession)
    :
    base_type(commonModule),
    m_authSession(authSession)
{
    setOnBeforeUpdatingSettingValue(
        [this](
            const QString& name,
            const QString& oldValue,
            const QString& newValue)
        {
            if (name == nx::settings_names::kNameSystemName)
                systemNameChanged(oldValue, newValue);
        });
}

void SystemSettingsProcessor::systemNameChanged(
    const QString& oldValue,
    const QString& newValue)
{
    if (oldValue == newValue)
        return;

    QnAuditRecord auditRecord = commonModule()->auditManager()->prepareRecord(m_authSession, Qn::AR_SystemNameChanged);
    QString description = lit("%1 -> %2").arg(oldValue).arg(newValue);
    auditRecord.addParam("description", description.toUtf8());
    commonModule()->auditManager()->addAuditRecord(auditRecord);
}

//-------------------------------------------------------------------------------------------------

nx::network::rest::Response SystemSettingsHandler::executeGet(
    const nx::network::rest::Request& request)
{
    if (!nx::network::rest::ini().allowGetModifications && request.params().isEmpty())
        return nx::network::rest::Response::error(nx::network::rest::Result::Forbidden);

    return executePost(request);
}

nx::network::rest::Response SystemSettingsHandler::executePost(
    const nx::network::rest::Request& request)
{
    nx::network::rest::JsonResult result;
    bool status = updateSettings(
        request.owner->commonModule(),
        request.params(),
        result,
        request.owner->accessRights(),
        request.owner->authSession());

    auto response = nx::network::rest::Response::result(result);
    response.statusCode = status
        ? nx::network::http::StatusCode::ok
        : nx::network::http::StatusCode::forbidden;

    return response;
}

bool SystemSettingsHandler::updateSettings(
    QnCommonModule* commonModule,
    const nx::network::rest::Params& params,
    nx::network::rest::JsonResult& result,
    const Qn::UserAccessData& accessRights,
    const QnAuthSession& authSession)
{
    SystemSettingsProcessor systemSettingsProcessor(commonModule, authSession);
    return systemSettingsProcessor.updateSettings(accessRights, authSession, params, &result) ==
        nx::network::http::StatusCode::ok;
}

} // namespace nx::vms::server
