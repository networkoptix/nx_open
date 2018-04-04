#include "common_plugin.h"

#define NX_DEBUG_ENABLE_OUTPUT m_enableOutput
#define NX_PRINT_PREFIX (std::string("[") + m_name + "] ")
#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace metadata {

CommonPlugin::CommonPlugin(const char* name):
    m_name(name)
{
    NX_PRINT << "Created " << this;
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
    return m_name;
}

void CommonPlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
   // TODO: Here roSettings are passed from Server. Currently, they are not used by metadata
   // plugins, thus, do nothing.
}

bool CommonPlugin::fillSettingsMap(
    std::map<std::string, std::string>* map, const nxpl::Setting* settings, int count,
    const char* func) const
{
    if (count > 0 && settings == nullptr)
    {
        NX_PRINT << func << "(): INTERNAL ERROR: settings is null and count is " << count;
        return false;
    }

    NX_OUTPUT << "{";
    for (int i = 0; i < count; ++i)
    {
        (*map)[settings[i].name] = settings[i].value;
        NX_OUTPUT << "    " << nx::kit::debug::toString(settings[i].name)
            << ": " << nx::kit::debug::toString(settings[i].value)
            << ((i < count - 1) ? "," : "");
    }
    NX_OUTPUT << "}";

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
