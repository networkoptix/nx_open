#include "common_engine.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->utils.enableOutput)
#include <nx/kit/debug.h>

#include <nx/sdk/utils.h>

namespace nx {
namespace sdk {
namespace analytics {

CommonEngine::CommonEngine(
    const std::string& pluginName,
    const std::string& libName,
    bool enableOutput,
    const std::string& printPrefix)
    :
    utils(enableOutput, !printPrefix.empty() ? printPrefix : ("[" + libName + "] ")),
    m_pluginName(pluginName),
    m_libName(libName)
{
    NX_PRINT << "Created " << this << ": \"" << m_pluginName << "\"";
}

std::string CommonEngine::getParamValue(const char* paramName)
{
    if (paramName == nullptr)
        return "";
    return m_settings[paramName];
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
    if (interfaceId == nxpl::IID_Plugin3)
    {
        addRef();
        return static_cast<nxpl::Plugin3*>(this);
    }
    if (interfaceId == nxpl::IID_Plugin2)
    {
        addRef();
        return static_cast<nxpl::Plugin2*>(this);
    }
    if (interfaceId == nxpl::IID_Plugin)
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

const char* CommonEngine::name() const
{
    // NOTE: Because Engine implements Plugin (which is not ideal, and can be changed in future),
    // its name() method should return the plugin name.
    return m_pluginName.c_str();
}

void CommonEngine::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Here roSettings are passed from Server. Currently, they are not used by metadata plugins,
    // thus, do nothing.
}

void CommonEngine::setDeclaredSettings(const nxpl::Setting* settings, int count)
{
    if (!utils.fillAndOutputSettingsMap(&m_settings, settings, count, "Received settings"))
        return; //< The error is already logged.

    settingsChanged();
}

void CommonEngine::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
}

void CommonEngine::setLocale(const char* /*locale*/)
{
}

const char* CommonEngine::manifest(Error* /*error*/) const
{
    m_manifest = manifest();
    return m_manifest.c_str();
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
        &params, action->params(), action->paramCount(), "params", /*outputIndent*/ 4))
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

} // namespace analytics
} // namespace sdk
} // namespace nx
