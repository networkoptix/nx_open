#include "device_agent.h"

#include <utils/common/synctime.h>
#include <media_server/media_server_module.h>
#include <plugins/vms_server_plugins_ini.h>
#include <core/resource/camera_resource.h>

#include <nx/utils/log/log.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log_level.h>
#include <nx/utils/log/log_main.h>
#include <nx/fusion/model_functions.h>

#include <nx/sdk/analytics/i_engine.h>

#include <nx/vms/server/sdk_support/utils.h>
#include <nx/vms/server/sdk_support/to_string.h>
#include <nx/vms/server/sdk_support/result_holder.h>
#include <nx/vms/server/analytics/wrappers/helpers.h>
#include <nx/vms/server/event/event_connector.h>
#include <nx/vms/server/interactive_settings/json_engine.h>

namespace nx::vms::server::analytics::wrappers {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::vms::api::analytics;
using namespace nx::vms::server::sdk_support;
using namespace nx::vms::event;

static nx::sdk::Ptr<const nx::sdk::IStringMap> mergeWithDbAndDefaultSettings(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine,
    const QVariantMap& settingsFromUser)
{
    const auto engineManifest = engine->manifest();
    interactive_settings::JsonEngine jsonEngine;
    jsonEngine.loadModelFromJsonObject(engineManifest.deviceAgentSettingsModel);

    const auto settingsFromProperty = device->deviceAgentSettingsValues(engine->getId());
    jsonEngine.applyValues(settingsFromProperty);
    jsonEngine.applyValues(settingsFromUser);

    return sdk_support::toIStringMap(jsonEngine.values());
}

#define RETURN_DEFAULT_VALUE(ReturnType) \
    if constexpr (std::is_same_v<std::nullopt_t, ReturnType>) \
        return std::nullopt; \
    else if constexpr (std::is_same_v<void, ReturnType>) \
        return; \
    else \
        return ReturnType{};

static const QString kManifestLogPrefix = "Getting DeviceAgent manifest";

//-------------------------------------------------------------------------------------------------

DeviceAgent::DeviceAgent(QnMediaServerModule* serverModule): ServerModuleAware(serverModule)
{
}

DeviceAgent::DeviceAgent(
    QnMediaServerModule* serverModule,
    nx::sdk::Result<IDeviceAgent*> deviceAgentResult,
    resource::AnalyticsEngineResourcePtr engine,
    QnVirtualCameraResourcePtr device)
    :
    ServerModuleAware(serverModule),
    m_engine(engine),
    m_device(device)
{
    const ResultHolder<IDeviceAgent*> result{std::move(deviceAgentResult)};

    if (!NX_ASSERT(m_engine))
        return;

    if (!NX_ASSERT(m_device))
        return;

    connect(this, &DeviceAgent::pluginDiagnosticEventTriggered,
        serverModule->eventConnector(), &event::EventConnector::at_pluginDiagnosticEvent,
        Qt::QueuedConnection);

    static const QString kLogPrefix = "Fetching DeviceAgent";
    if (!result.isOk())
    {
        handleError<void>(Error::fromResultHolder(result), kLogPrefix);
        return;
    }

    if (m_deviceAgent = result.value(); !m_deviceAgent)
    {
        handleContractViolation<void>(
            api::PredefinedPluginEventType::nullDeviceAgent,
            kLogPrefix);
        return;
    }

    m_consumingDeviceAgent = queryInterfacePtr<IConsumingDeviceAgent>(m_deviceAgent);
}

QString DeviceAgent::makeCaption(api::PredefinedPluginEventType violationType) const
{
    // TODO: #dmishin rewrite it.
    return QString("Contract violation: (caption): ") + QnLexical::serialized(violationType);
}

QString DeviceAgent::makeDescription(api::PredefinedPluginEventType violationType) const
{
    // TODO: #dmishin rewrite it.
    return QString("Contract violation (description): ") + QnLexical::serialized(violationType);
}

QString DeviceAgent::makeCaption(const sdk_support::Error& error) const
{
    // TODO: #dmishin rewrite it.
    return QString("Error (caption): ") + toString(error);
}

QString DeviceAgent::makeDescription(const sdk_support::Error& error) const
{
    // TODO: #dmishin rewrite it.
    return QString("Error (description): ") + toString(error);
}

template<typename ReturnType>
ReturnType DeviceAgent::handleContractViolation(
    api::PredefinedPluginEventType violationType,
    const QString& prefix) const
{
    logContractViolation(violationType, prefix);
    throwPluginEvent(makeCaption(violationType), makeDescription(violationType));

    RETURN_DEFAULT_VALUE(ReturnType);
}

template<typename ReturnType>
ReturnType DeviceAgent::handleError(const sdk_support::Error& error, const QString& prefix) const
{
    NX_ASSERT(!error.isOk());
    logError(error, prefix);
    throwPluginEvent(makeCaption(error), makeDescription(error));

    RETURN_DEFAULT_VALUE(ReturnType);
}

void DeviceAgent::logError(const sdk_support::Error& error, const QString& prefix) const
{
    const QString logMessageBegginingTemplate = prefix.isEmpty()
        ? "Error occurred "
        : prefix + ": error occurred ";

    NX_DEBUG(this, logMessageBegginingTemplate + "(%1), device: %2, engine %3",
        error, m_device, m_engine);
}

void DeviceAgent::logContractViolation(
    api::PredefinedPluginEventType violationType,
    const QString& prefix) const
{
    const QString logMessageBegginingTemplate = prefix.isEmpty()
        ? "Interface contract has been violated "
        : prefix + ": interface contract has been violated ";
    NX_DEBUG(this, logMessageBegginingTemplate + "(%1), device %2, engine (%3)",
        violationType, m_device, m_engine);
}

void DeviceAgent::throwPluginEvent(const QString& caption, const QString& description) const
{
    PluginDiagnosticEventPtr pluginDiagnosticEvent(
        new PluginDiagnosticEvent(
            qnSyncTime->currentUSecsSinceEpoch(),
            m_engine->getId(),
            caption,
            description,
            nx::vms::api::EventLevel::ErrorEventLevel,
            m_device));

    emit pluginDiagnosticEventTriggered(pluginDiagnosticEvent);
}

Ptr<IEngine> DeviceAgent::engine() const
{
    // TODO: Be carefull since the engine() method doesn't involve reference counting.
    if (!NX_ASSERT(m_deviceAgent))
        return nullptr;

    // TODO: handle errors properly.
    return toPtr(m_deviceAgent->engine());
}

std::optional<ErrorMap> DeviceAgent::setSettings(const SettingMap& settings)
{
    if (!NX_ASSERT(m_deviceAgent))
        return std::nullopt;

    Ptr<const IStringMap> sdkSettings = prepareSettings(settings);
    if (!NX_ASSERT(sdkSettings))
        return std::nullopt;

    const ResultHolder<const IStringMap*> result =
        m_deviceAgent->setSettings(sdkSettings.get());

    if (!result.isOk())
    {
        return handleError<std::nullopt_t>(
            Error::fromResultHolder(result),
            "Passing settings to the DeviceAgent");
    }

    return fromSdkStringMap(result.value());
}

std::optional<SettingsResponse> DeviceAgent::pluginSideSettings() const
{
    if (!NX_ASSERT(m_deviceAgent))
        return std::nullopt;

    const ResultHolder<const ISettingsResponse*> result = m_deviceAgent->pluginSideSettings();
    if (!result.isOk())
    {
        return handleError<std::nullopt_t>(
            Error::fromResultHolder(result),
            "Getting plugin side settings from DeviceAgent");
    }

    const auto sdkSettingsResponse = result.value();
    if (!sdkSettingsResponse)
    {
        NX_DEBUG(this,
            "Got a null settings response while obtaining device agent settings "
            "for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());

        return std::nullopt;
    }

    const auto settingValues = toPtr(sdkSettingsResponse->values());
    if (!settingValues)
    {
        NX_DEBUG(this,
            "Got a null settings value map while obtaining device agent settings "
            "for device %1 (%2) and engine %3 (%4)",
            m_device->getUserDefinedName(),
            m_device->getId(),
            m_engine->getName(),
            m_engine->getId());
    }

    SettingsResponse settingsResponse{
        fromSdkStringMap<QVariantMap>(settingValues),
        fromSdkStringMap(toPtr(sdkSettingsResponse->errors()))};

    return settingsResponse;
}

Ptr<const IStringMap> DeviceAgent::prepareSettings(const SettingMap& settings) const
{
    const auto sdkSettings = makePtr<StringMap>();
    for (const auto& [key, value]: nx::utils::constKeyValueRange(settings))
        sdkSettings->setItem(key.toStdString(), value.toString().toStdString());

    if (pluginsIni().analyticsSettingsOutputPath[0] != '\0')
    {
        debug_helpers::dumpStringToFile(
            this,
            QString::fromStdString(toJsonString(sdkSettings.get())),
            pluginsIni().analyticsSettingsOutputPath,
            debug_helpers::nameOfFileToDumpOrLoadData(
                m_device,
                m_engine,
                resource::AnalyticsPluginResourcePtr(),
                "_effective_settings.json"));
    }

    return sdkSettings;
}

std::optional<api::analytics::DeviceAgentManifest> DeviceAgent::manifest() const
{
    if (!NX_ASSERT(m_deviceAgent))
        return std::nullopt;

    Ptr<const IString> manifest = loadManifestFromFile();
    if (!manifest)
        manifest = loadManifestFromDeviceAgent();

    dumpManifestToFileIfNeeded(manifest);
    return validateManifest(manifest);
}

Ptr<const IString> DeviceAgent::loadManifestFromFile() const
{
    if (pluginsIni().analyticsManifestSubstitutePath[0] == '\0')
        return nullptr;

    return sdk_support::loadManifestStringFromFile(
        m_device,
        m_engine,
        resource::AnalyticsPluginResourcePtr()
        /* TODO: add logger */);
}

Ptr<const IString> DeviceAgent::loadManifestFromDeviceAgent() const
{
    const ResultHolder<const IString*> result = m_deviceAgent->manifest();
    if (!result.isOk())
        return handleError<nullptr_t>(Error::fromResultHolder(result), kManifestLogPrefix);

    return result.value();
}

void DeviceAgent::dumpManifestToFileIfNeeded(const Ptr<const IString> manifestString) const
{
    if (pluginsIni().analyticsManifestOutputPath[0] == '\0')
        return;

    const QString str = (manifestString && manifestString->str())
        ? QString(manifestString->str())
        : QString();

    // TODO: improve logging, maybe pass a separate logger to this func.
    analytics::debug_helpers::dumpStringToFile(
        nx::utils::log::Tag(QString("nx::vms::server::analytics::wrappers::DeviceAgent")),
        str,
        QString(pluginsIni().analyticsManifestOutputPath),
        analytics::debug_helpers::nameOfFileToDumpOrLoadData(
            m_device,
            m_engine,
            resource::AnalyticsPluginResourcePtr(),
            "_manifest.json"));
}

std::optional<api::analytics::DeviceAgentManifest> DeviceAgent::validateManifest(
    const Ptr<const IString> manifestString) const
{
    if (!manifestString)
    {
        return handleContractViolation<std::nullopt_t>(
            api::PredefinedPluginEventType::nullDeviceAgentManifest,
            kManifestLogPrefix);
    }

    const char* const manifestCString = manifestString->str();
    if (!manifestCString)
    {
        return handleContractViolation<std::nullopt_t>(
            api::PredefinedPluginEventType::nullDeviceAgentManifestString,
            kManifestLogPrefix);
    }

    if (*manifestCString == '\0')
    {
        return handleContractViolation<std::nullopt_t>(
            api::PredefinedPluginEventType::emptyDeviceAgentManifestString,
            kManifestLogPrefix);
    }

    bool success = false;
    const auto manifest =
        QJson::deserialized<api::analytics::DeviceAgentManifest>(manifestCString, {}, &success);

    if (!success)
    {
        return handleContractViolation<std::nullopt_t>(
            api::PredefinedPluginEventType::deviceAgentManifestDeserializationError,
            kManifestLogPrefix);
    }

    return manifest;
}

void DeviceAgent::setHandler(Ptr<IDeviceAgent::IHandler> handler)
{
    if (!NX_ASSERT(m_deviceAgent))
        return;

    m_deviceAgent->setHandler(handler.get());
}

bool DeviceAgent::setNeededMetadataTypes(const MetadataTypes& metadataTypes)
{
    if (!NX_ASSERT(m_deviceAgent))
        return false;

    const ResultHolder<void> result =
        m_deviceAgent->setNeededMetadataTypes(toSdkMetadataTypes(metadataTypes).get());

    if (!result.isOk())
    {
        return handleError<bool>(
            Error::fromResultHolder(result),
            "Setting needed metadata types to DeviceAgent");
    }

    return true;
}

bool DeviceAgent::pushDataPacket(Ptr<IDataPacket> data)
{
    if (!NX_ASSERT(m_consumingDeviceAgent))
        return false;

    const ResultHolder<void> result = m_consumingDeviceAgent->pushDataPacket(data.get());
    if (!result.isOk())
    {
        return handleError<bool>(
            Error::fromResultHolder(result),
            "Pushing data packet to DeviceAgent");
    }

    return true;
}

} // namespace nx::vms::server::analytics::wrappers
