#include "execute_analytics_action_rest_handler.h"

#include <api/model/analytics_actions.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <media_server/media_server_module.h>
#include <nx/vms/server/analytics/manager.h>
#include <nx/vms/server/resource/analytics_engine_resource.h>
#include <nx/vms/server/resource/analytics_plugin_resource.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/vms_server_plugins/utils/uuid.h>
#include <nx/vms/server/sdk_support/utils.h>
#include <nx/sdk/i_string_map.h>

#include <plugins/settings.h>
#include <plugins/plugin_tools.h>

using namespace nx::vms::server;

QnExecuteAnalyticsActionRestHandler::QnExecuteAnalyticsActionRestHandler(
    QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

QStringList QnExecuteAnalyticsActionRestHandler::cameraIdUrlParams() const
{
    return {"deviceId"};
}

int QnExecuteAnalyticsActionRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* /*owner*/)
{
    using namespace nx::vms::server;
    bool success = false;
    const auto actionData =
        QJson::deserialized<AnalyticsAction>(body, AnalyticsAction(), &success);
    if (!success)
    {
        result.setError(QnJsonRestResult::InvalidParameter, lit("Invalid Json object provided"));
        return nx::network::http::StatusCode::ok;
    }

    QString missedField;
    if (actionData.engineId.isNull())
        missedField = "engineId";
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

    const auto engineResource =
        resourcePool()->getResourceById<resource::AnalyticsEngineResource>(actionData.engineId);

    if (!engineResource)
    {
        NX_WARNING(this, "Engine with id %1 not found", actionData.engineId);
        result.setError(QnJsonRestResult::InvalidParameter, "Engine not found");
        return nx::network::http::StatusCode::ok;
    }

    AnalyticsActionResult actionResult;
    QString errorMessage = executeAction(
        &actionResult, engineResource->sdkEngine().get(), actionData);

    if (!errorMessage.isEmpty())
    {
        result.setError(QnJsonRestResult::CantProcessRequest,
            lit("Plugin failed to execute action: %1").arg(errorMessage));
    }
    result.setReply(actionResult);
    return nx::network::http::StatusCode::ok;
}

namespace {

class Action: public nxpt::CommonRefCounter<nx::sdk::analytics::IAction>
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override
    {
        if (interfaceId == nx::sdk::analytics::IID_Action)
        {
            addRef();
            return static_cast<nx::sdk::analytics::IAction*>(this);
        }
        if (interfaceId == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    Action(const AnalyticsAction& actionData, AnalyticsActionResult* actionResult):
        m_params(nx::vms::server::sdk_support::toIStringMap(actionData.params)),
        m_actionResult(actionResult)
    {
        NX_ASSERT(m_actionResult);

        m_actionId = actionData.actionId.toStdString();
        m_objectId = nx::vms_server_plugins::utils::fromQnUuidToPluginGuid(actionData.objectId);
        m_deviceId = nx::vms_server_plugins::utils::fromQnUuidToPluginGuid(actionData.deviceId);
        m_timestampUs = actionData.timestampUs;
    }

    virtual const char* actionId() override { return m_actionId.c_str(); }

    virtual nxpl::NX_GUID objectId() override { return m_objectId; }

    virtual nxpl::NX_GUID deviceId() override { return m_deviceId; }

    virtual int64_t timestampUs() override { return m_timestampUs; }

    virtual const nx::sdk::IStringMap* params() override { return m_params.get(); }

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

    const nx::sdk::Ptr<nx::sdk::IStringMap> m_params;

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
        default: return "unrecognized error";
    }
}

} // namespace

/**
 * @return Null on success, or a short error message on error.
 */
QString QnExecuteAnalyticsActionRestHandler::executeAction(
    AnalyticsActionResult* outActionResult,
    nx::sdk::analytics::IEngine* engine,
    const AnalyticsAction& actionData)
{
    Action action(actionData, outActionResult);

    nx::sdk::Error error = nx::sdk::Error::noError;
    engine->executeAction(&action, &error);

    // By this time, the Engine either already called Action::handleResult(), or is not going to
    // do it.
    return errorMessage(error);
}

std::unique_ptr<sdk_support::AbstractManifestLogger>
    QnExecuteAnalyticsActionRestHandler::makeLogger(
        resource::AnalyticsEngineResourcePtr engineResource) const
{
    const QString messageTemplate(
        "Error occurred while fetching Engine manifest for engine: {:engine}: {:error}");

    return std::make_unique<sdk_support::ManifestLogger>(
        typeid(this), //< Using the same tag for all instances.
        messageTemplate,
        std::move(engineResource));
}

