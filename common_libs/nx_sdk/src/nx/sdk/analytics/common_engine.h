#pragma once

#include <string>
#include <map>

#include <plugins/plugin_tools.h>
#include <nx/sdk/utils.h>
#include <nx/sdk/settings.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/objects_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Base class for a typical implementation of an Analytics Engine. Hides many technical details of
 * the Analytics Plugin SDK, but may limit plugin capabilities - use only when suitable.
 *
 * To use NX_PRINT/NX_OUTPUT in a derived class with the same printPrefix as used in this class,
 * add the following to the derived class cpp:
 * <pre><code>
 *     #define NX_PRINT_PREFIX (this->utils.printPrefix)
 *     #include <nx/kit/debug.h>
 * </code></pre>
 */
class CommonEngine: public nxpt::CommonRefCounter<Engine>
{
protected:
    nx::sdk::Utils utils;

protected:
    /**
     * @param pluginName Full plugin name in English, capitalized as a header.
     * @param libName Short plugin name: small_letters_and_underscores, as in the library filename.
     * @param enableOutput Enables NX_OUTPUT. Typically, use NX_DEBUG_ENABLE_OUTPUT as a value.
     * @param printPrefix Prefix for NX_PRINT and NX_OUTPUT. If empty, will be made from libName.
     */
    CommonEngine(
        Plugin* plugin,
        bool enableOutput,
        const std::string& printPrefix = "");

    virtual std::string manifest() const = 0;

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

    /**
     * Action handler. Called when some action defined by this engine is triggered by Server.
     * @param actionId Id of the action being triggered.
     * @param objectId Id of a metadata object for which the action has been triggered.
     * @param params If the engine manifest defines params for the action being triggered,
     *     contains their values after they are filled by the user via Client form. Otherwise,
     *     empty.
     * @param outActionUrl If set by this call, Client will open this URL in an embedded browser.
     * @param outMessageToUser If set by this call, Client will show this text to the user.
     */
    virtual void executeAction(
        const std::string& /*actionId*/,
        nxpl::NX_GUID /*objectId*/,
        nxpl::NX_GUID /*deviceId*/,
        int64_t /*timestampUs*/,
        const std::map<std::string, std::string>& /*params*/,
        std::string* /*outActionUrl*/,
        std::string* /*outMessageToUser*/,
        Error* /*error*/)
    {
    }

    /**
     * Intended to be called from a method of a derived class overriding plugin().
     * @return Parent Plugin, casted to the specified type.
     */
    template<typename DerivedPlugin>
    DerivedPlugin* pluginCasted()
    {
        const auto plugin= dynamic_cast<DerivedPlugin*>(m_plugin);
        assetPluginCasted(plugin);
        return plugin;
    }

public:
    virtual ~CommonEngine() override;

    virtual Plugin* plugin() const override { return m_plugin; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual void setSettings(const nx::sdk::Settings* settings) override;
    virtual nx::sdk::Settings* settings() const override;
    virtual const char* manifest(Error* error) const override;
    void freeManifest(const char* data) override;

    virtual void executeAction(Action* action, Error* outError) override;

private:
    void assertPluginCasted(void* plugin) const;

private:
    Plugin* const m_plugin;
    std::map<std::string, std::string> m_settings;
    mutable std::string m_manifest; //< Cache the manifest to guarantee a lifetime for char*.
};

} // namespace analytics
} // namespace sdk
} // namespace nx
