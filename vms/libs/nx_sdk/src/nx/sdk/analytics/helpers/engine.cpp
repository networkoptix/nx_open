#include "engine.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->logUtils.enableOutput)
#include <nx/kit/debug.h>
#include <nx/kit/utils.h>

#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/plugin_event.h>
#include <nx/sdk/analytics/i_plugin.h>

namespace nx {
namespace sdk {
namespace analytics {

class PrintPrefixMaker
{
public:
    std::string makePrintPrefix(
        const std::string& overridingPrintPrefix,
        const IPlugin* plugin,
        const IEngineInfo* engineInfo = nullptr)
    {
        NX_KIT_ASSERT(plugin);
        NX_KIT_ASSERT(plugin->name());

        if (!overridingPrintPrefix.empty())
            return overridingPrintPrefix;

        std::string printPrefix = std::string("[") + plugin->name() + "_engine";
        if (engineInfo)
            printPrefix += std::string("_") + engineInfo->id();

        printPrefix += "] ";
        return printPrefix;
    }

private:
    /** Used by the above NX_KIT_ASSERT (via NX_PRINT). */
    struct
    {
        const std::string printPrefix = "nx::sdk::analytics::Engine(): ";
    } logUtils;
};

Engine::Engine(
    IPlugin* plugin,
    bool enableOutput,
    const std::string& printPrefix)
    :
    logUtils(enableOutput, PrintPrefixMaker().makePrintPrefix(printPrefix, plugin)),
    m_plugin(plugin),
    m_overridingPrintPrefix(printPrefix)
{
    NX_PRINT << "Created " << this << ": \"" << plugin->name() << "\"";
}

std::string Engine::getParamValue(const char* paramName)
{
    if (paramName == nullptr)
        return "";
    return m_settings[paramName];
}

void Engine::pushPluginEvent(
    IPluginEvent::Level level,
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

    const auto event = makePtr<PluginEvent>(
        level, std::move(caption), std::move(description));
    m_handler->handlePluginEvent(event.get());
}

Engine::~Engine()
{
    NX_PRINT << "Destroyed " << this;
}

void Engine::setEngineInfo(const IEngineInfo* engineInfo)
{
    logUtils.setPrintPrefix(
        PrintPrefixMaker().makePrintPrefix(m_overridingPrintPrefix, m_plugin, engineInfo));
}

void Engine::setSettings(const IStringMap* settings)
{
    if (!logUtils.convertAndOutputStringMap(&m_settings, settings, "Received settings"))
        return; //< The error is already logged.

    settingsReceived();
}

IStringMap* Engine::pluginSideSettings() const
{
    auto settingsValues = new StringMap();
    settingsValues->addItem("nx.stub.engine.settings.double_0", "2.7182");
    return settingsValues;
}

const IString* Engine::manifest(Error* /*error*/) const
{
    return new String(manifest());
}

void Engine::executeAction(IAction* action, Error* outError)
{
    if (!action)
    {
        NX_PRINT << __func__ << "(): INTERNAL ERROR: action is null";
        *outError = Error::unknownError;
        return;
    }

    std::map<std::string, std::string> params;

    NX_OUTPUT << __func__ << "():";
    NX_OUTPUT << "{";
    NX_OUTPUT << "    actionId: " << nx::kit::utils::toString(action->actionId());
    NX_OUTPUT << "    objectId: " << action->objectId();
    NX_OUTPUT << "    deviceId: " << action->deviceId();
    NX_OUTPUT << "    timestampUs: " << action->timestampUs();

    const auto actionParams = toPtr(action->params());

    if (!logUtils.convertAndOutputStringMap(
        &params, actionParams.get(), "params", /*outputIndent*/ 4))
    {
        // The error is already logged.
        *outError = Error::unknownError;
        return;
    }

    NX_OUTPUT << "}";

    std::string actionUrl;
    std::string messageToUser;

    executeAction(
        action->actionId(),
        action->objectId(),
        action->deviceId(),
        action->timestampUs(),
        nx::sdk::toPtr(action->objectTrackInfo()),
        params,
        &actionUrl,
        &messageToUser,
        outError);

    const char* const actionUrlPtr = actionUrl.empty() ? nullptr : actionUrl.c_str();
    const char* const messageToUserPtr = messageToUser.empty() ? nullptr : messageToUser.c_str();
    action->handleResult(actionUrlPtr, messageToUserPtr);
}

Error Engine::setHandler(IEngine::IHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    handler->addRef();
    m_handler.reset(handler);
    return Error::noError;
}

bool Engine::isCompatible(const IDeviceInfo* /*deviceInfo*/) const
{
    return true;
}

void Engine::assertPluginCasted(void* plugin) const
{
    // This method is placed in .cpp to allow NX_KIT_ASSERT() use the correct NX_PRINT() prefix.
    NX_KIT_ASSERT(plugin,
        "nx::sdk::analytics::Engine " + nx::kit::utils::toString(this)
        + " has m_plugin of incorrect runtime type " + typeid(*m_plugin).name());
}

} // namespace analytics
} // namespace sdk
} // namespace nx
