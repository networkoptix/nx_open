#include "execute_analytics_action_rest_handler.h"

#include <api/model/analytics_actions.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <media_server/media_server_module.h>
#include <nx/mediaserver/analytics/manager.h>
#include <nx/mediaserver/resource/analytics_engine_resource.h>
#include <nx/mediaserver/resource/analytics_plugin_resource.h>
#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver_plugins/utils/uuid.h>
#include <nx/mediaserver/sdk_support/utils.h>
#include <nx/sdk/settings.h>

#include <plugins/settings.h>
#include <plugins/plugin_tools.h>

QnExecuteAnalyticsActionRestHandler::QnExecuteAnalyticsActionRestHandler(
    QnMediaServerModule* serverModule):
    nx::mediaserver::ServerModuleAware(serverModule)
{
}

int QnExecuteAnalyticsActionRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    using namespace nx::mediaserver;
    bool success = false;
    const auto actionData =
        QJson::deserialized<AnalyticsAction>(body, AnalyticsAction(), &success);
    if (!success)
    {
        result.setError(QnJsonRestResult::InvalidParameter, lit("Invalid Json object provided"));
        return nx::network::http::StatusCode::ok;
    }

    QString missedField;
    if (actionData.pluginId.isEmpty())
        missedField = "pluginId";
    else if (actionData.actionId.isEmpty())
        missedField = "actionId";
    else if (actionData.objectId.isNull())
        missedField = "objectId";
    if (!missedField.isEmpty())
    {
        result.setError(
            QnJsonRestResult::InvalidParameter,
            lit("Missing required field '%1'").arg(missedField));
        return nx::network::http::StatusCode::ok;
    }

    const auto engines = resourcePool()->getResources<resource::AnalyticsEngineResource>();
    for (auto& engine: engines)
    {
        auto sdkEngine = engine->sdkEngine();
        auto manifest = sdk_support::manifest<nx::vms::api::analytics::EngineManifest>(sdkEngine);
        if (!manifest)
        {
            NX_WARNING(this, lm("Can't obtain engine manifest, engine %1 (%2)")
                .args(engine->getName(), engine->getId()));
            continue;
        }

        if (manifest->pluginId == actionData.pluginId)
        {
            AnalyticsActionResult actionResult;
            QString errorMessage = executeAction(&actionResult, sdkEngine.get(), actionData);

            if (!errorMessage.isEmpty())
            {
                result.setError(QnJsonRestResult::CantProcessRequest,
                    lit("Plugin failed to execute action: %1").arg(errorMessage));
            }
            result.setReply(actionResult);
            return nx::network::http::StatusCode::ok;
        }
    }

    result.setError(QnJsonRestResult::CantProcessRequest,
        lit("Plugin with pluginId \"%1\" not found").arg(actionData.pluginId));
    return nx::network::http::StatusCode::ok;
}

namespace {

class Action: public nxpt::CommonRefCounter<nx::sdk::analytics::Action>
{
public:
    virtual ~Action() {}

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override
    {
        if (interfaceId == nx::sdk::analytics::IID_Action)
        {
            addRef();
            return static_cast<nx::sdk::analytics::Action*>(this);
        }
        if (interfaceId == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    Action(const AnalyticsAction& actionData, AnalyticsActionResult* actionResult):
        m_params(
            nx::mediaserver::sdk_support::toSdkSettings(actionData.params)),
        m_actionResult(actionResult)
    {
        NX_ASSERT(m_actionResult);

        m_actionId = actionData.actionId.toStdString();
        m_objectId = nx::mediaserver_plugins::utils::fromQnUuidToPluginGuid(actionData.objectId);
        m_deviceId = nx::mediaserver_plugins::utils::fromQnUuidToPluginGuid(actionData.cameraId);
        m_timestampUs = actionData.timestampUs;
    }

    virtual const char* actionId() override { return m_actionId.c_str(); }

    virtual nxpl::NX_GUID objectId() override { return m_objectId; }

    virtual nxpl::NX_GUID deviceId() override { return m_deviceId; }

    virtual int64_t timestampUs() override { return m_timestampUs; }

    virtual const nx::sdk::Settings* params() override { return m_params.get(); }

    virtual int paramCount() override { return m_params->count(); }

    virtual void handleResult(const char* actionUrl, const char* messageToUser) override
    {
        m_actionResult->actionUrl = actionUrl;
        m_actionResult->messageToUser = messageToUser;
    }

private:
    std::string m_actionId;
    nxpl::NX_GUID m_objectId;
    nxpl::NX_GUID m_deviceId;
    int64_t m_timestampUs;

    const nx::mediaserver::sdk_support::UniquePtr<nx::sdk::Settings> m_params;

    AnalyticsActionResult* m_actionResult = nullptr;
};

/**
 * @return Null if no error.
 */
QString errorMessage(nx::sdk::Error error)
{
    switch (error)
    {
        case nx::sdk::Error::noError: return QString();
        case nx::sdk::Error::unknownError: return "error";
        case nx::sdk::Error::networkError: return "network error";
        case nx::sdk::Error::typeIsNotSupported: return "type is not supported";
        default: return "unrecognized error";
    }
}

} // namespace

/**
 * @return Null on success, or a short error message on error.
 */
QString QnExecuteAnalyticsActionRestHandler::executeAction(
    AnalyticsActionResult* outActionResult,
    nx::sdk::analytics::Engine* engine,
    const AnalyticsAction& actionData)
{
    Action action(actionData, outActionResult);

    nx::sdk::Error error = nx::sdk::Error::noError;
    engine->executeAction(&action, &error);

    // By this time, engine either already called Action::handleResult(), or is not going to do it.

    return errorMessage(error);
}
