/**********************************************************
* Dec 11, 2015
* a.kolesnikov
***********************************************************/

#include "system_settings_handler.h"

#include <api/global_settings.h>
#include <api/resource_property_adaptor.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>
#include <api/model/system_settings_reply.h>
#include <audit/audit_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <transaction/transaction_descriptor.h>
#include <core/resource_access/resource_access_manager.h>
#include <common/common_module.h>

int QnSystemSettingsHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    bool status = updateSettings(owner->commonModule(), params, result, owner->accessRights(), owner->authSession());
    return status ? nx_http::StatusCode::ok : nx_http::StatusCode::forbidden;
}

bool QnSystemSettingsHandler::updateSettings(
    QnCommonModule* commonModule,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const Qn::UserAccessData& accessRights,
    const QnAuthSession& authSession)
{
    QnSystemSettingsReply reply;
    bool dirty = false;
    const auto& settings = commonModule->globalSettings()->allSettings();

    QnRequestParams filteredParams(params);
    filteredParams.remove("auth");

    namespace ahlp = ec2::access_helpers;

    for (QnAbstractResourcePropertyAdaptor* setting: settings)
    {
        bool readAllowed = ahlp::kvSystemOnlyFilter(
            ahlp::Mode::read,
            accessRights,
            setting->key());

        bool writeAllowed = ec2::access_helpers::kvSystemOnlyFilter(
            ahlp::Mode::write,
            accessRights,
            setting->key());

        writeAllowed &= commonModule->resourceAccessManager()->hasGlobalPermission(
            accessRights,
            Qn::GlobalPermission::GlobalAdminPermission);

        if (!filteredParams.isEmpty())
        {
            auto paramIter = filteredParams.find(setting->key());
            if (paramIter == filteredParams.end())
                continue;

            if (!writeAllowed)
                return false;

            if (setting->key() == nx::settings_names::kNameSystemName)
                systemNameChanged(authSession, setting->serializedValue(), paramIter.value());

            setting->setSerializedValue(paramIter.value());
            dirty = true;
        }

        if (readAllowed)
            reply.settings.insert(setting->key(), setting->serializedValue());
    }
    if (dirty)
        commonModule->globalSettings()->synchronizeNow();

    result.setReply(std::move(reply));
    return true;
}

void QnSystemSettingsHandler::systemNameChanged(
    const QnAuthSession& authSession,
    const QString& oldValue,
    const QString& newValue)
{
    if (oldValue == newValue)
        return;

    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(authSession, Qn::AR_SystemNameChanged);
    QString description = lit("%1 -> %2").arg(oldValue).arg(newValue);
    auditRecord.addParam("description", description.toUtf8());
    qnAuditManager->addAuditRecord(auditRecord);
}
