#include "common_engine.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->utils.enableOutput)
#include <nx/kit/debug.h>

#include <nx/sdk/utils.h>
#include <nx/sdk/common_settings.h>
#include <nx/sdk/common/string.h>

#include <nx/sdk/common/plugin_event.h>

#include "plugin.h"

namespace nx {
namespace sdk {
namespace analytics {

class PrintPrefixMaker
{
public:
    std::string makePrintPrefix(const std::string& overridingPrintPrefix, const Plugin* plugin)
    {
        NX_KIT_ASSERT(plugin);
        NX_KIT_ASSERT(plugin->name());

        if (!overridingPrintPrefix.empty())
            return overridingPrintPrefix;
        return std::string("[") + plugin->name() + " Engine] ";
    }

private:
    /** Used by the above NX_KIT_ASSERT (via NX_PRINT). */
    struct
    {
        const std::string printPrefix = "CommonEngine(): ";
    } utils;
};

CommonEngine::CommonEngine(
    Plugin* plugin,
    bool enableOutput,
    const std::string& printPrefix)
    :
    utils(enableOutput, PrintPrefixMaker().makePrintPrefix(printPrefix, plugin)),
    m_plugin(plugin)
{
    NX_PRINT << "Created " << this << ": \"" << plugin->name() << "\"";
}

std::string CommonEngine::getParamValue(const char* paramName)
{
    if (paramName == nullptr)
        return "";
    return m_settings[paramName];
}

void CommonEngine::pushPluginEvent(
    nx::sdk::IPluginEvent::Level level,
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

    nxpt::ScopedRef<IPluginEvent> event(
        new common::PluginEvent(level, std::move(caption), std::move(description)),
        /*increaseRef*/ false);

    m_handler->handlePluginEvent(event.get());
}

CommonEngine::~CommonEngine()
{
    NX_PRINT << "Destroyed " << this;
}

void* CommonEngine::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Engine)
    {
        addRef();
        return static_cast<Engine*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

void CommonEngine::setSettings(const nx::sdk::Settings* settings)
{
    if (!utils.fillAndOutputSettingsMap(&m_settings, settings, "Received settings"))
        return; //< The error is already logged.

    settingsChanged();
}

nx::sdk::Settings* CommonEngine::pluginSideSettings() const
{
    auto settingsValues = new nx::sdk::CommonSettings();
    settingsValues->addSetting("nx.stub.engine.settings.double_0", "2.7182");
    return settingsValues;
}

const IString* CommonEngine::manifest(Error* /*error*/) const
{
    return new common::String(manifest());
}

void CommonEngine::executeAction(Action* action, Error* outError)
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
    NX_OUTPUT << "    actionId: " << nx::kit::debug::toString(action->actionId());
    NX_OUTPUT << "    objectId: " << action->objectId();
    NX_OUTPUT << "    deviceId: " << action->deviceId();
    NX_OUTPUT << "    timestampUs: " << action->timestampUs();

    if (!utils.fillAndOutputSettingsMap(
        &params, action->params(), "params", /*outputIndent*/ 4))
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
        params,
        &actionUrl,
        &messageToUser,
        outError);

    const char* const actionUrlPtr = actionUrl.empty() ? nullptr : actionUrl.c_str();
    const char* const messageToUserPtr = messageToUser.empty() ? nullptr : messageToUser.c_str();
    action->handleResult(actionUrlPtr, messageToUserPtr);
}

nx::sdk::Error CommonEngine::setHandler(nx::sdk::analytics::Engine::IHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
    return nx::sdk::Error::noError;
}

void CommonEngine::assertPluginCasted(void* plugin) const
{
    NX_KIT_ASSERT(plugin,
        "CommonEngine " + nx::kit::debug::toString(this)
        + " has m_plugin of incorrect runtime type " + typeid(*m_plugin).name());
}

} // namespace analytics
} // namespace sdk
} // namespace nx
