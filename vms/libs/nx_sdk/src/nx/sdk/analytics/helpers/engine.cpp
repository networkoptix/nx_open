// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <nx/kit/utils.h>
#include <nx/sdk/analytics/i_integration.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/integration_diagnostic_event.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/lib_context.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#undef NX_DEBUG_ENABLE_OUTPUT
#define NX_DEBUG_ENABLE_OUTPUT (this->logUtils.enableOutput)
#include <nx/kit/debug.h>

namespace nx::sdk::analytics {

static std::string makePrintPrefix(
    const std::string& integrationInstanceId, const IEngineInfo* engineInfo = nullptr)
{
    const std::string& integrationInstanceIdCaption =
        integrationInstanceId.empty() ? "" : ("_" + integrationInstanceId);

    return "[" + libContext().name() + integrationInstanceIdCaption + "_engine"
        + (!engineInfo ? "" : (std::string("_") + engineInfo->id())) + "] ";
}

Engine::Engine(bool enableOutput, const std::string& integrationInstanceId):
    logUtils(enableOutput, makePrintPrefix(integrationInstanceId)),
    m_integrationInstanceId(integrationInstanceId)
{
    NX_PRINT << "Created IEngine @" << nx::kit::utils::toString(this)
        << " of " << libContext().name();
}

// TODO: Consider making a template with param type, checked according to the manifest.
std::string Engine::settingValue(const std::string& settingName) const
{
    const auto it = m_settings.find(settingName);
    if (it != m_settings.end())
        return it->second;

    NX_PRINT << "ERROR: Requested setting "
        << nx::kit::utils::toString(settingName) << " is missing; implying empty string.";
    return "";
}

std::map<std::string, std::string> Engine::currentSettings() const
{
    return m_settings;
}

Result<IAction::Result> Engine::executeAction(
    const std::string& /*actionId*/,
    Uuid /*objectTrackId*/,
    Uuid /*deviceId*/,
    int64_t /*timestampUs*/,
    Ptr<IObjectTrackInfo> /*trackInfo*/,
    const std::map<std::string, std::string>& /*params*/)
{
    return {};
}

void Engine::pushIntegrationDiagnosticEvent(
    IIntegrationDiagnosticEvent::Level level,
    std::string caption,
    std::string description) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handler)
    {
        NX_PRINT << __func__ << "(): INTERNAL ERROR: "
            << "setHandler() was not called; ignoring the Integration Diagnostic Event.";
        return;
    }

    const auto event = makePtr<IntegrationDiagnosticEvent>(
        level, std::move(caption), std::move(description));

    NX_OUTPUT << "Producing Integration Diagnostic Event:\n" + event->toString();

    m_handler->handleIntegrationDiagnosticEvent(event.get());
}

void Engine::pushManifest(const std::string& manifest)
{
    const auto manifestSdkString = nx::sdk::makePtr<nx::sdk::String>(manifest);
    m_handler->pushManifest(manifestSdkString.get());
}

Engine::~Engine()
{
    NX_PRINT << "Destroyed " << this;
}

void Engine::setEngineInfo(const IEngineInfo* engineInfo)
{
    logUtils.setPrintPrefix(makePrintPrefix(m_integrationInstanceId, engineInfo));
}

void Engine::doSetSettings(
    Result<const ISettingsResponse*>* outResult, const IStringMap* settings)
{
    if (!logUtils.convertAndOutputStringMap(&m_settings, settings, "Received settings"))
        *outResult = error(ErrorCode::invalidParams, "Unable to convert the input string map");
    else
        *outResult = settingsReceived();
}

void Engine::getIntegrationSideSettings(Result<const ISettingsResponse*>* /*outResult*/) const
{
}

void Engine::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new String(manifestString());
}

void Engine::doExecuteAction(Result<IAction::Result>* outResult, const IAction* action)
{
    if (!action)
    {
        NX_PRINT << __func__ << "(): INTERNAL ERROR: action is null";
        *outResult = error(ErrorCode::invalidParams, "Action is null");
        return;
    }

    std::map<std::string, std::string> params;

    NX_OUTPUT << __func__ << "():";
    NX_OUTPUT << "{";
    NX_OUTPUT << "    actionId: " << nx::kit::utils::toString(action->actionId());
    NX_OUTPUT << "    objectTrackId: " << action->objectTrackId();
    NX_OUTPUT << "    deviceId: " << action->deviceId();
    NX_OUTPUT << "    timestampUs: " << action->timestampUs();

    const auto actionParams = action->params();

    if (!logUtils.convertAndOutputStringMap(
        &params, actionParams.get(), "params", /*outputIndent*/ 4))
    {
        // The error is already logged.
        *outResult = error(ErrorCode::invalidParams, "Invalid action parameters");
        return;
    }

    NX_OUTPUT << "}";

    *outResult = executeAction(
        action->actionId(),
        action->objectTrackId(),
        action->deviceId(),
        action->timestampUs(),
        action->objectTrackInfo(),
        params);
}

void Engine::setHandler(IEngine::IHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = shareToPtr(handler);
}

bool Engine::isCompatible(const IDeviceInfo* /*deviceInfo*/) const
{
    return true;
}

void Engine::doGetSettingsOnActiveSettingChange(
    Result<const IActiveSettingChangedResponse*>* /*outResult*/,
    const IActiveSettingChangedAction* /*activeSettingChangedAction*/)
{
}

} // namespace nx::sdk::analytics
