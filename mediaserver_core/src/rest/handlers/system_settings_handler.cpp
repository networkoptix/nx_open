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
#include <rest/server/rest_connection_processor.h>
#include <transaction/transaction_descriptor.h>

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
        bool allowed;
        ec2::access_helpers::globalSettingsSystemOnlyFilter(owner->accessRights(), setting->key(), &allowed);

        if (!params.isEmpty())
        {
            auto paramIter = params.find(setting->key());
            if (paramIter == params.end())
                continue;

            if (!allowed)
                return nx_http::StatusCode::forbidden;

            setting->setSerializedValue(paramIter.value());
            dirty = true;
        }

        if (allowed)
            reply.settings.insert(setting->key(), setting->serializedValue());
        else 
            reply.settings.insert(setting->key(), QString());
    }
    if (dirty)
        QnGlobalSettings::instance()->synchronizeNow();

    result.setReply(std::move(reply));
    return nx_http::StatusCode::ok;
}
