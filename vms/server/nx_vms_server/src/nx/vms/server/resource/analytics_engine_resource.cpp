#include "analytics_engine_resource.h"

#include <media_server/media_server_module.h>
#include <plugins/vms_server_plugins_ini.h>

#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/vms/server/sdk_support/result_holder.h>

#include <nx/vms/server/analytics/settings_engine_wrapper.h>
#include <nx/vms/server/analytics/wrappers/engine.h>
#include <nx/vms/api/analytics/descriptors.h>

#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/analytics/helpers/engine_info.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/sdk/i_string_map.h>
#include <nx/utils/member_detector.h>
#include <nx/analytics/descriptor_manager.h>

namespace nx::vms::server::resource {

using namespace nx::vms::api::analytics;
using namespace nx::vms::server::analytics;

static const QString kSettingsModelProperty("settingsModel");

template<typename T>
using ResultHolder = nx::vms::server::sdk_support::ResultHolder<T>;

AnalyticsEngineResource::AnalyticsEngineResource(QnMediaServerModule* serverModule):
    base_type(),
    ServerModuleAware(serverModule)
{
}

void AnalyticsEngineResource::setSdkEngine(wrappers::EnginePtr sdkEngine)
{
    m_sdkEngine = std::move(sdkEngine);
}

wrappers::EnginePtr AnalyticsEngineResource::sdkEngine() const
{
    return m_sdkEngine;
}

SettingsResponse AnalyticsEngineResource::getSettings()
{
    if (!m_sdkEngine)
    {
        NX_WARNING(this, "Getting settings, unable to access the SDK Engine object, Engine %1",
            toSharedPointer(this));

        return SettingsResponse(
            SettingsResponse::Error::Code::sdkObjectIsNotAccessible,
            "Engine is not accessible");
    }

    const std::optional<sdk_support::SdkSettingsResponse> sdkSettingsResponse =
        m_sdkEngine->pluginSideSettings();

    if (!sdkSettingsResponse)
    {
        NX_DEBUG(this, "Unable to obtain an SDK settings response, Engine: %1",
            toSharedPointer(this));

        return SettingsResponse(SettingsResponse::Error::Code::unableToObtainSettingsResponse);
    }

    const SettingsContext settingsContext = updateSettingsContext(
        currentSettingsContext(), api::analytics::SettingsValues(), *sdkSettingsResponse);

    return prepareSettingsResponse(settingsContext, *sdkSettingsResponse);
}

SettingsResponse AnalyticsEngineResource::setSettings(const SetSettingsRequest& settingsRequest)
{
    NX_DEBUG(this, "Got set settings request, Engine: %1", toSharedPointer(this));
    return setSettingsInternal(settingsRequest, /*isInitialSettings*/ false);
}

analytics::SettingsResponse AnalyticsEngineResource::setSettings()
{
    SetSettingsRequest setSettingsRequest;
    setSettingsRequest.values = storedSettingsValues();
    setSettingsRequest.modelId = calculateModelId(storedSettingsModel());

    NX_DEBUG(this, "Setting settings, taking stored values, Engine: %1", toSharedPointer(this));

    return setSettingsInternal(setSettingsRequest, /*isInitialSettings*/ false);
}

SettingsContext AnalyticsEngineResource::currentSettingsContext() const
{
    SettingsContext settingsContext;
    settingsContext.model = storedSettingsModel();
    settingsContext.modelId = calculateModelId(settingsContext.model);
    settingsContext.values = storedSettingsValues();

    NX_DEBUG(this, "Current settings context, model: %1, model id: %2, values: %3, Engine: %4",
        settingsContext.model, settingsContext.modelId, settingsContext.values,
        toSharedPointer(this));

    return settingsContext;
}

SettingsContext AnalyticsEngineResource::updateSettingsContext(
    const SettingsContext& settingsContext,
    const api::analytics::SettingsValues& requestValues,
    const sdk_support::SdkSettingsResponse& sdkSettingsResponse)
{
    NX_DEBUG(this, "Updating settings context, current settings model: %1, "
        "current settings model id: %2, settings values from the request: %3, "
        "settings model from the SDK response: %4, settings values from the SDK response: %5 "
        "Device: %6",
         settingsContext.model, settingsContext.modelId, requestValues,
         (sdkSettingsResponse.model ? toString(*sdkSettingsResponse.model) : "<null>"),
         (sdkSettingsResponse.values ? toString(*sdkSettingsResponse.values) : "<null>"),
         toSharedPointer(this));

    SettingsContext result = settingsContext;
    if (sdkSettingsResponse.model)
    {
        NX_DEBUG(this,
            "Updating settings context, updating current settings model from the SDK response, "
            "Engine: %1",
            toSharedPointer(this));

        result.model = *sdkSettingsResponse.model;
    }

    result.modelId = calculateModelId(result.model);

    analytics::SettingsEngineWrapper settingsEngine(
        serverModule()->eventConnector(), toSharedPointer(this));

    settingsEngine.loadModelFromJsonObject(result.model);

    // Applying current settings values.
    settingsEngine.applyValues(settingsContext.values);

    // Applying values from the settings request.
    settingsEngine.applyValues(requestValues);

    // Overriding values with ones from the SDK response (if any).
    if (sdkSettingsResponse.values)
    {
        NX_DEBUG(this,
            "Updating settings context, applying settings values from the SDK response, "
            "Engine: %1",
            toSharedPointer(this));

        settingsEngine.applyStringValues(*sdkSettingsResponse.values);
    }

    result.values = settingsEngine.values();

    NX_DEBUG(this,
        "Updating settings context, resulting settings values: %1, Engine: %2",
        result.values, toSharedPointer(this));

    setSettingsModel(result.model);
    setSettingsValues(result.values);

    saveProperties();

    return result;
}

api::analytics::SettingsValues AnalyticsEngineResource::prepareSettings(
    const SettingsContext& settingsContext,
    const api::analytics::SettingsValues& settingsValues) const
{
    NX_DEBUG(this,
        "Preparing settings, settings values: %1, settings values from "
        "the current settings context %2, settings model from the current settings context: %3, "
        "Engine %4",
        settingsValues, settingsContext.values, settingsContext.model, toSharedPointer(this));

    analytics::SettingsEngineWrapper settingsEngine(
        serverModule()->eventConnector(), toSharedPointer(this));
    settingsEngine.loadModelFromJsonObject(settingsContext.model);
    settingsEngine.applyValues(settingsContext.values);
    settingsEngine.applyValues(settingsValues);

    const api::analytics::SettingsValues result = settingsEngine.values();
    NX_DEBUG(this, "Preparing settings, resulting values: %1, Engine: %2",
        result, toSharedPointer(this));

    return result;
}

SettingsResponse AnalyticsEngineResource::prepareSettingsResponse(
    const SettingsContext& settingsContext,
    const sdk_support::SdkSettingsResponse& sdkSettingsResponse) const
{
    SettingsResponse result;

    analytics::SettingsEngineWrapper settingsEngine(
        serverModule()->eventConnector(), toSharedPointer(this));
    settingsEngine.loadModelFromJsonObject(settingsContext.model);

    result.model = settingsEngine.serializeModel();
    result.modelId = settingsContext.modelId;
    result.values = settingsContext.values;
    result.errors = sdkSettingsResponse.errors;

    // TODO: stringify SDK error code
    if (!sdkSettingsResponse.sdkError.isOk())
    {
        NX_DEBUG(this,
            "The SDK response contains an SDK error: %1, message: %2, Engine: %3",
            sdkSettingsResponse.sdkError.errorCode, sdkSettingsResponse.sdkError.errorMessage,
            toSharedPointer(this));

        result.error = SettingsResponse::Error(
            SettingsResponse::Error::Code::sdkError,
            sdkSettingsResponse.sdkError.errorMessage);
    }

    NX_DEBUG(this,
        "Resulting settings response, settings model: %1, settings model id: %2, "
        "settings values: %3, settings errors: %4, Engine: %5",
        result.model, result.modelId, result.values, result.errors, toSharedPointer(this));

    return result;
}

api::analytics::SettingsValues AnalyticsEngineResource::storedSettingsValues() const
{
    return QJsonDocument::fromJson(getProperty(kSettingsValuesProperty).toUtf8()).object();
}

api::analytics::SettingsModel AnalyticsEngineResource::storedSettingsModel() const
{
    const QString settingsModelString = getProperty(kSettingsModelProperty);
    if (!settingsModelString.isEmpty())
    {
        NX_DEBUG(this, "Settings model from the property: %1, Engine: %2",
            settingsModelString, toSharedPointer(this));

        bool success = false;
        sdk_support::SettingsModel deserializedSettingsModel;

        deserializedSettingsModel = QJson::deserialized(
            settingsModelString.toUtf8(), api::analytics::SettingsModel(), &success);

        if (success)
            return deserializedSettingsModel;

        NX_DEBUG(this, "Unable to deserialize the settings model from property, Engine: %1",
            toSharedPointer(this));
    }

    std::optional<PluginManifest> parentPluginManifest = pluginManifest();
    if (!NX_ASSERT(parentPluginManifest))
        api::analytics::SettingsModel();

     return parentPluginManifest->engineSettingsModel;
}

void AnalyticsEngineResource::setSettingsValues(
    const api::analytics::SettingsValues& settingsValues)
{
    NX_DEBUG(this, "Saving settings values to property, values: %1, Engine: %2",
        settingsValues, toSharedPointer(this));

    setProperty(
        kSettingsValuesProperty,
        QString::fromUtf8(QJsonDocument(settingsValues).toJson()));
}

void AnalyticsEngineResource::setSettingsModel(const api::analytics::SettingsModel& settingsModel)
{
    NX_DEBUG(this, "Saving settings model to property, model: %1, Engine: %2",
        settingsModel, toSharedPointer(this));

    setProperty(kSettingsModelProperty, QString::fromUtf8(QJsonDocument(settingsModel).toJson()));
}

analytics::SettingsResponse AnalyticsEngineResource::setSettingsInternal(
    const analytics::SetSettingsRequest& settingsRequest,
    bool isInitialSettings)
{
    NX_DEBUG(this,
        "Setting settings, settings values: %1, settings model id: %2, isInitialSettings: %3, "
        "Engine: %5",
        settingsRequest.values, settingsRequest.modelId, isInitialSettings, toSharedPointer(this));

    if (!m_sdkEngine)
    {
        NX_DEBUG(this, "Settings settings, unable to access the SDK Engine, %1",
            toSharedPointer(this));

        return SettingsResponse::Error(
            SettingsResponse::Error::Code::sdkObjectIsNotAccessible,
            "Engine is not accessible");
    }

    SettingsContext settingsContext = currentSettingsContext();
    #if 0 //< Enable this section when the Engine settings dialog supports modelId
        if ((settingsContext.modelId != settingsRequest.modelId) && !isInitialSettings)
            return SettingsResponse::Error(SettingsResponse::Error::Code::wrongSettingsModel);
    #endif

    const std::optional<sdk_support::SdkSettingsResponse> sdkSettingsResponse =
        m_sdkEngine->setSettings(
            isInitialSettings
                ? settingsRequest.values
                : prepareSettings(settingsContext, settingsRequest.values));

    if (!sdkSettingsResponse)
    {
        NX_DEBUG(this, "Setting settings, unable to obtain an SDK settings response, Engine %1",
            toSharedPointer(this));

        return SettingsResponse(SettingsResponse::Error::Code::unableToObtainSettingsResponse);
    }

    settingsContext = updateSettingsContext(
        settingsContext,
        settingsRequest.values,
        *sdkSettingsResponse);

    return prepareSettingsResponse(settingsContext, *sdkSettingsResponse);
}

std::optional<nx::vms::api::analytics::PluginManifest>
    AnalyticsEngineResource::pluginManifest() const
{
    auto parentPlugin = plugin().dynamicCast<resource::AnalyticsPluginResource>();
    if (!parentPlugin)
        return std::nullopt;

    return parentPlugin->manifest();
}

CameraDiagnostics::Result AnalyticsEngineResource::initInternal()
{
    NX_DEBUG(this, lm("Initializing Analytics Engine resource %1 (%2)").args(getName(), getId()));

    if (!m_sdkEngine)
        return CameraDiagnostics::InternalServerErrorResult("SDK Engine object is not set");

    NX_DEBUG(this, "Sending engine info to the Engine %1 (%2)", getName(), getId());

    auto engineInfo = nx::sdk::makePtr<nx::sdk::analytics::EngineInfo>();
    engineInfo->setId(getId().toStdString());
    engineInfo->setName(getName().toStdString());
    m_sdkEngine->setEngineInfo(engineInfo);

    m_handler = nx::sdk::makePtr<analytics::EngineHandler>(serverModule(), getId());
    m_sdkEngine->setHandler(m_handler);

    auto parentPlugin = plugin().dynamicCast<resource::AnalyticsPluginResource>();
    if (!parentPlugin)
        return CameraDiagnostics::PluginErrorResult("Can't find parent Plugin");

    const auto pluginManifest = parentPlugin->manifest();
    setSettingsModel(pluginManifest.engineSettingsModel);

    SetSettingsRequest initialSettingsRequest;
    initialSettingsRequest.values = storedSettingsValues();

    if (initialSettingsRequest.values.isEmpty())
    {
        // If there are no Settings in the database we use default Settings from the Model.
        // Otherwise we pass the Settings from the database without pulling them through the Model.
        SettingsEngineWrapper settingsEngine(
            serverModule()->eventConnector(),
            toSharedPointer(this));

        settingsEngine.loadModelFromJsonObject(storedSettingsModel());
        initialSettingsRequest.values = settingsEngine.values();
    }

    const analytics::SettingsResponse settingsResponse =
        setSettingsInternal(initialSettingsRequest, /*isInitialSettings*/ true);

    if (!settingsResponse.error.isOk())
        return CameraDiagnostics::InternalServerErrorResult("Unable to send settings to Engine");

    const auto manifest = m_sdkEngine->manifest();
    if (!manifest)
        return CameraDiagnostics::PluginErrorResult("Can't obtain Engine Manifest");

    nx::analytics::DescriptorManager descriptorManager(commonModule());
    descriptorManager.updateFromEngineManifest(pluginManifest.id, getId(), getName(), *manifest);

    setManifest(*manifest);
    saveProperties();

    emit engineInitialized(toSharedPointer(this));

    return CameraDiagnostics::NoErrorResult();
}

QString AnalyticsEngineResource::libName() const
{
    if (!NX_ASSERT(m_sdkEngine))
        return QString();

    return m_sdkEngine->libName();
}

} // nx::vms::server::resource
