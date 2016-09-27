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

        if (!params.isEmpty())
        {
            auto paramIter = params.find(setting->key());
            if (paramIter == params.end())
                continue;

            if (!writeAllowed)
                return nx_http::StatusCode::forbidden;

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
