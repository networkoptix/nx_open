#include "system_settings_handler.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>

#include <api/global_settings.h>
#include <api/resource_property_adaptor.h>
#include <api/model/system_settings_reply.h>
#include <audit/audit_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>

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

    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(m_authSession, Qn::AR_SystemNameChanged);
    QString description = lit("%1 -> %2").arg(oldValue).arg(newValue);
    auditRecord.addParam("description", description.toUtf8());
    qnAuditManager->addAuditRecord(auditRecord);
}

//-------------------------------------------------------------------------------------------------

int QnSystemSettingsHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    bool status = updateSettings(
        owner->commonModule(),
        params,
        result,
        owner->accessRights(),
        owner->authSession());
    return status ? nx_http::StatusCode::ok : nx_http::StatusCode::forbidden;
}

bool QnSystemSettingsHandler::updateSettings(
    QnCommonModule* commonModule,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const Qn::UserAccessData& accessRights,
    const QnAuthSession& authSession)
{
    SystemSettingsProcessor systemSettingsProcessor(commonModule, authSession);
    return systemSettingsProcessor.updateSettings(accessRights, authSession, params, &result) ==
        nx_http::StatusCode::ok;
}
