/**********************************************************
* Dec 11, 2015
* a.kolesnikov
***********************************************************/

#include "system_settings_handler.h"

#include <api/global_settings.h>
#include <api/resource_property_adaptor.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/httptypes.h>
#include <api/model/system_settings_reply.h>
#include <audit/audit_manager.h>
#include <rest/server/rest_connection_processor.h>
#include <transaction/transaction_descriptor.h>
#include <core/resource_access/resource_access_manager.h>

int QnSystemSettingsHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnSystemSettingsReply reply;
    bool dirty = false;
    const auto& settings = QnGlobalSettings::instance()->allSettings();

    namespace ahlp = ec2::access_helpers;

    for (QnAbstractResourcePropertyAdaptor* setting: settings)
    {
        bool readAllowed = ahlp::kvSystemOnlyFilter(
            ahlp::Mode::read,
            owner->accessRights(),
            setting->key());

        bool writeAllowed = ec2::access_helpers::kvSystemOnlyFilter(
            ahlp::Mode::write,
            owner->accessRights(),
            setting->key());

        writeAllowed &= qnResourceAccessManager->hasGlobalPermission(
            owner->accessRights(),
            Qn::GlobalPermission::GlobalAdminPermission);

        if (!params.isEmpty())
        {
            auto paramIter = params.find(setting->key());
            if (paramIter == params.end())
                continue;

            if (!writeAllowed)
                return nx_http::StatusCode::forbidden;

            if (setting->key() == nx::settings_names::kNameSystemName)
                systemNameChanged(owner, setting->serializedValue(), paramIter.value());

            setting->setSerializedValue(paramIter.value());
            dirty = true;
        }

        if (readAllowed)
            reply.settings.insert(setting->key(), setting->serializedValue());
    }
    if (dirty)
        QnGlobalSettings::instance()->synchronizeNow();

    result.setReply(std::move(reply));
    return nx_http::StatusCode::ok;
}

void QnSystemSettingsHandler::systemNameChanged(
    const QnRestConnectionProcessor* owner,
    const QString& oldValue,
    const QString& newValue)
{
    if (oldValue == newValue)
        return;

    QnAuditRecord auditRecord = qnAuditManager->prepareRecord(owner->authSession(), Qn::AR_SystemNameChanged);
    QString description = lit("%1 -> %2").arg(oldValue).arg(newValue);
    auditRecord.addParam("description", description.toUtf8());
    qnAuditManager->addAuditRecord(auditRecord);
}
