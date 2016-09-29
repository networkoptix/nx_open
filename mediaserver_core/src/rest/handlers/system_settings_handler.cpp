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

int QnSystemSettingsHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    QnSystemSettingsReply reply;

    bool dirty = false;
    const auto& settings = QnGlobalSettings::instance()->allSettings();
    for (QnAbstractResourcePropertyAdaptor* setting: settings)
    {
        if (!params.isEmpty())
        {
            auto paramIter = params.find(setting->key());
            if (paramIter == params.end())
                continue;

            if (setting->key() == QnGlobalSettings::kNameSystemName)
                systemNameChanged(owner, setting->serializedValue(), paramIter.value());

            setting->setSerializedValue(paramIter.value());
            dirty = true;
        }
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
