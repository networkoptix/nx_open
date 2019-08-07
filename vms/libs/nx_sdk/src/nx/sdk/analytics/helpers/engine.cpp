// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->logUtils.enableOutput)
#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include <nx/sdk/analytics/i_plugin.h>

#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/lib_context.h>

namespace nx {
namespace sdk {
namespace analytics {

static std::string makePrintPrefix(
    const std::string& overridingPrintPrefix,
    const IEngineInfo* engineInfo = nullptr)
{
    if (!overridingPrintPrefix.empty())
        return overridingPrintPrefix;

    std::string printPrefix = std::string("[") + libContext().name() + "_engine";
    if (engineInfo)
        printPrefix += std::string("_") + engineInfo->id();

    printPrefix += "] ";
    return printPrefix;
}

Engine::Engine(
    bool enableOutput,
    const std::string& printPrefix)
    :
    logUtils(enableOutput, makePrintPrefix(printPrefix)),
    m_overridingPrintPrefix(printPrefix)
{
    NX_PRINT << "Created " << this << ": \"" << libContext().name() << "\"";
}

std::string Engine::settingValue(const std::string& settingName)
{
    return m_settings[settingName];
}

void Engine::pushPluginDiagnosticEvent(
    IPluginDiagnosticEvent::Level level,
    std::string caption,
    std::string description)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handler)
    {
        NX_PRINT << __func__ << "(): "
            << "INTERNAL ERROR: setHandler() was not called; ignoring plugin event";
        return;
    }

    const auto event = makePtr<PluginDiagnosticEvent>(
        level, std::move(caption), std::move(description));
    m_handler->handlePluginDiagnosticEvent(event.get());
}

Engine::~Engine()
{
    NX_PRINT << "Destroyed " << this;
}

void Engine::setEngineInfo(const IEngineInfo* engineInfo)
{
    logUtils.setPrintPrefix(makePrintPrefix(m_overridingPrintPrefix, engineInfo));
}

void Engine::doSetSettings(Result<const IStringMap*>* outResult, const IStringMap* settings)
{
    if (!logUtils.convertAndOutputStringMap(&m_settings, settings, "Received settings"))
        *outResult = error(ErrorCode::invalidParams, "Unable to convert the input string map");
    else
        *outResult = settingsReceived();
}

void Engine::getPluginSideSettings(Result<const ISettingsResponse*>* /*outResult*/) const
{
}

void Engine::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new String(manifestString());
}

void Engine::doExecuteAction(Result<void>* outResult, IAction* action)
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

    std::string actionUrl;
    std::string messageToUser;

    const auto result = executeAction(
        action->actionId(),
        action->objectTrackId(),
        action->deviceId(),
        action->timestampUs(),
        action->objectTrackInfo(),
        params,
        &actionUrl,
        &messageToUser);

    const char* const actionUrlPtr = actionUrl.empty() ? nullptr : actionUrl.c_str();
    const char* const messageToUserPtr = messageToUser.empty() ? nullptr : messageToUser.c_str();
    action->handleResult(actionUrlPtr, messageToUserPtr);

    *outResult = result;
}

void Engine::setHandler(IEngine::IHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    handler->addRef();
    m_handler.reset(handler);
}

bool Engine::isCompatible(const IDeviceInfo* /*deviceInfo*/) const
{
    return true;
}

} // namespace analytics
} // namespace sdk
} // namespace nx
