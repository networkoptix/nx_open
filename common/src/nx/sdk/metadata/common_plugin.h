#pragma once

#include <string>
#include <map>

#include <plugins/plugin_tools.h>

#include "plugin.h"
#include "objects_metadata_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Base class for a typical implementation of the metadata plugin. Hides many technical details of
 * the Metadata Plugin SDK, but may limit plugin capabilities - use only when suitable.
 */
class CommonPlugin: public nxpt::CommonRefCounter<Plugin>
{
protected:
    CommonPlugin(const char* name);

    virtual std::string capabilitiesManifest() const = 0;

    /**
     * Called when any of the seetings (param values) change.
     */
    virtual void settingsChanged() {}

    /**
     * Provides access to the Plugin global settings stored by the server.
     * @return Param value, or an empty string if such param does not exist, having logged the
     * error.
     */
    std::string getParamValue(const char* paramName);

    /** Enable or disable verbose debug output via NX_OUTPUT from methods of this class. */
    void setEnableOutput(bool value) { m_enableOutput = value; }

    /**
     * Action handler. Called when some action defined by this plugin is triggered by Server.
     * @param actionId Id of the action being triggered.
     * @param objectId Id of a metadata object for which the action has been triggered.
     * @param params If the plugin manifest defines params for the action being triggered,
     *     contains their values after they are filled by the user via Client form. Otherwise,
     *     empty.
     * @param outActionUrl If set by this call, Client will open this URL in an embedded browser.
     * @param outMessageToUser If set by this call, Client will show this text to the user.
     */
    virtual void executeAction(
        const std::string& /*actionId*/,
        nxpl::NX_GUID /*objectId*/,
        const std::map<std::string, std::string>& /*params*/,
        std::string* /*outActionUrl*/,
        std::string* /*outMessageToUser*/,
        Error* /*error*/)
    {
    }

public:
    virtual ~CommonPlugin() override;

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual const char* name() const override;
    virtual void setSettings(const nxpl::Setting* settings, int count) override;
    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;
    virtual void setLocale(const char* locale) override;
    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;
    virtual const char* capabilitiesManifest(Error* error) const override;
    virtual void executeAction(Action* action, Error* outError) override;

private:
    bool CommonPlugin::fillSettingsMap(
        std::map<std::string, std::string>* map, const nxpl::Setting* settings, int count,
        const char* func) const;

private:
    bool m_enableOutput = false;
    const char* const m_name;
    std::map<std::string, std::string> m_settings;
    mutable std::string m_manifest; //< Cache the manifest to guarantee a lifetime for char*.
};

} // namespace metadata
} // namespace sdk
} // namespace nx
