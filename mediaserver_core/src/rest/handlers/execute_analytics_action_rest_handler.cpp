#include "execute_analytics_action_rest_handler.h"

#include <api/model/analytics_actions.h>
#include <rest/server/rest_connection_processor.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <media_server/media_server_module.h>
#include <nx/mediaserver/metadata/manager_pool.h>
#include <nx/mediaserver_plugins/utils/uuid.h>
#include <nx/plugins/settings.h>

int QnExecuteAnalyticsActionRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    bool success = false;
    const auto actionData =
        QJson::deserialized<AnalyticsAction>(body, AnalyticsAction(), &success);
    if (!success)
    {
        result.setError(QnJsonRestResult::InvalidParameter, lit("Invalid Json object provided"));
        return nx::network::http::StatusCode::ok;
    }

    QString missedField;
    if (actionData.driverId.isNull())
        missedField = "driverId";
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

    auto managerPool = qnServerModule->metadataManagerPool();
    for (auto& plugin: managerPool->availablePlugins())
    {
        nxpt::ScopedRef<nx::sdk::metadata::Plugin> pluginGuard(plugin, /*increaseRef*/ false);

        const auto manifest = managerPool->loadPluginManifest(plugin);
        if (!manifest)
            continue; //< The error is already logged.

        if (manifest.get().driverId == actionData.driverId)
        {
            AnalyticsActionResult actionResult;
            QString errorMessage = executeAction(&actionResult, plugin, actionData);

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
        lit("Plugin with driverId %1 not found").arg(actionData.driverId.toString()));
    return nx::network::http::StatusCode::ok;
}

namespace {

class Action: public nxpt::CommonRefCounter<nx::sdk::metadata::Action>
{
public:
    virtual ~Action() {}

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override
    {
        if (interfaceId == nx::sdk::metadata::IID_Action)
        {
            addRef();
            return static_cast<nx::sdk::metadata::Action*>(this);
        }
        if (interfaceId == nxpl::IID_PluginInterface)
        {
            addRef();
            return static_cast<nxpl::PluginInterface*>(this);
        }
        return nullptr;
    }

    Action(const AnalyticsAction& actionData, AnalyticsActionResult* actionResult):
        m_params(actionData.params), m_actionResult(actionResult)
    {
        NX_ASSERT(m_actionResult);
        NX_ASSERT(m_params.isValid());

        m_actionId = actionData.actionId.toStdString();
        m_objectId = nx::mediaserver_plugins::utils::fromQnUuidToPluginGuid(actionData.objectId);
        m_cameraId = nx::mediaserver_plugins::utils::fromQnUuidToPluginGuid(actionData.cameraId);
        m_timestampUs = actionData.timestampUs;
    }

    virtual const char* actionId() override { return m_actionId.c_str(); }

    virtual nxpl::NX_GUID objectId() override { return m_objectId; }

    virtual nxpl::NX_GUID cameraId() override { return m_cameraId; }

    virtual int64_t timestampUs() override { return m_timestampUs; }

    virtual const nxpl::Setting* params() override { return m_params.array(); }

    virtual int paramCount() override { return m_params.size(); }

    virtual void handleResult(const char* actionUrl, const char* messageToUser) override
    {
        m_actionResult->actionUrl = actionUrl;
        m_actionResult->messageToUser = messageToUser;
    }

private:
    std::string m_actionId;
    nxpl::NX_GUID m_objectId;
    nxpl::NX_GUID m_cameraId;
    int64_t m_timestampUs;

    nx::plugins::SettingsHolder m_params;

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
    nx::sdk::metadata::Plugin* plugin,
    const AnalyticsAction& actionData)
{
    Action action(actionData, outActionResult);

    nx::sdk::Error error = nx::sdk::Error::noError;
    plugin->executeAction(&action, &error);

    // By this time, Plugin either already called Action::handleResult(), or is not going to do it.

    return errorMessage(error);
}
