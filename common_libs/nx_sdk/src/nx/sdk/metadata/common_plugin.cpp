#include "common_plugin.h"

#define NX_PRINT_PREFIX printPrefix()
#define NX_DEBUG_ENABLE_OUTPUT enableOutput()
#include <nx/kit/debug.h>

#include <nx/sdk/utils/debug.h>

namespace nx {
namespace sdk {
namespace metadata {

CommonPlugin::CommonPlugin(
    const std::string& name,
    const std::string& libName,
    bool enableOutput_,
    const std::string& printPrefix_)
    :
    Debug(enableOutput_, !printPrefix_.empty() ? printPrefix_ : ("[" + libName + "] ")),
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
   // TODO: Here roSettings are passed from Server. Currently, they are not used by metadata
   // plugins, thus, do nothing.
}

bool CommonPlugin::fillSettingsMap(
    std::map<std::string, std::string>* map, const nxpl::Setting* settings, int count,
    const std::string& func) const
{
    if (!debugOutputSettings(settings, count, func + " settings"))
        return false;

    for (int i = 0; i < count; ++i)
        (*map)[settings[i].name] = settings[i].value;

    return true;
}

void CommonPlugin::setDeclaredSettings(const nxpl::Setting* settings, int count)
{
    NX_OUTPUT << "Received Plugin settings:";
    if (!fillSettingsMap(&m_settings, settings, count, __func__))
        return; //< Error is already logged.

    settingsChanged();
}

void CommonPlugin::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
    // Do nothing.
}

void CommonPlugin::setLocale(const char* /*locale*/)
{
    // Do nothing.
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

    // TODO: #mshevchenko: Move logging action details here from stub plugin.cpp.

    NX_OUTPUT << "Executing action with id [" << action->actionId() << "]";

    std::map<std::string, std::string> params;
    if (!fillSettingsMap(&params, action->params(), action->paramCount(), __func__))
    {
        // Error is already logged.
        *outError = Error::unknownError;
        return;
    }

    std::string actionUrl;
    std::string messageToUser;
    executeAction(
        action->actionId(), action->objectId(), action->cameraId(), action->timestampUs(), params,
        &actionUrl, &messageToUser, outError);

    const char* const actionUrlPtr = actionUrl.empty() ? nullptr : actionUrl.c_str();
    const char* const messageToUserPtr = messageToUser.empty() ? nullptr : messageToUser.c_str();
    action->handleResult(actionUrlPtr, messageToUserPtr);
}

} // namespace metadata
} // namespace sdk
} // namespace nx
