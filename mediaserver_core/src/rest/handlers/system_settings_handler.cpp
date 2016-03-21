/**********************************************************
* Dec 11, 2015
* a.kolesnikov
***********************************************************/

#include "system_settings_handler.h"

#include <api/global_settings.h>
#include <api/resource_property_adaptor.h>
#include <utils/common/model_functions.h>
#include <utils/network/http/httptypes.h>


class QnSystemSettingsReply
{
public:
    //map<name, value>
    std::map<QString, QString> settings;
};

#define QnSystemSettingsReply_Fields (settings)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnSystemSettingsReply),
    (json),
    _Fields)


int QnSystemSettingsHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor*)
{
    QnSystemSettingsReply reply;

    const auto& settings = QnGlobalSettings::instance()->allSettings();
    for (QnAbstractResourcePropertyAdaptor* setting: settings)
    {
        if (!params.isEmpty())
        {
            auto paramIter = params.find(setting->key());
            if (paramIter == params.end())
                continue;
            setting->setValue(paramIter.value());
        }
        reply.settings.emplace(setting->key(), setting->value().toString());
    }

    result.setReply(std::move(reply));
    return nx_http::StatusCode::ok;
}
