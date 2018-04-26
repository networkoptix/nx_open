#include "common_plugin.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->utils.enableOutput)
#include <nx/kit/debug.h>

#include <nx/sdk/utils.h>

namespace nx {
namespace sdk {
namespace metadata {

CommonPlugin::CommonPlugin(
    const std::string& name,
    const std::string& libName,
    bool enableOutput,
    const std::string& printPrefix)
    :
    utils(enableOutput, !printPrefix.empty() ? printPrefix : ("[" + libName + "] ")),
    m_name(name),
    m_libName(libName)
{
    NX_PRINT << "Created " << this << ": \"" << m_name << "\"";
}

std::string CommonPlugin::getParamValue(const char* paramName)
{
    if (paramName == nullptr)
        return "";
    return m_settings[paramName];
}

CommonPlugin::~CommonPlugin()
{
    NX_PRINT << "Destroyed " << this;
}

void* CommonPlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_Plugin)
    {
        addRef();
        return static_cast<Plugin*>(this);
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

const char* CommonPlugin::name() const
{
    return m_name.c_str();
}

void CommonPlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Here roSettings are passed from Server. Currently, they are not used by metadata plugins,
    // thus, do nothing.
}

void CommonPlugin::setDeclaredSettings(const nxpl::Setting* settings, int count)
{
    if (!utils.fillAndOutputSettingsMap(&m_settings, settings, count, "Received settings"))
        return; //< The error is already logged.

    settingsChanged();
}

void CommonPlugin::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
}

void CommonPlugin::setLocale(const char* /*locale*/)
{
}

const char* CommonPlugin::capabilitiesManifest(Error* /*error*/) const
{
    m_manifest = capabilitiesManifest();
    return m_manifest.c_str();
}

void CommonPlugin::executeAction(Action* action, Error* outError)
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
    NX_OUTPUT << "    cameraId: " << action->cameraId();
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
        action->cameraId(),
        action->timestampUs(),
        params,
        &actionUrl,
        &messageToUser,
        outError);

    const char* const actionUrlPtr = actionUrl.empty() ? nullptr : actionUrl.c_str();
    const char* const messageToUserPtr = messageToUser.empty() ? nullptr : messageToUser.c_str();
    action->handleResult(actionUrlPtr, messageToUserPtr);
}

} // namespace metadata
} // namespace sdk
} // namespace nx
