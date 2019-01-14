#pragma once

#include <string>
#include <map>
#include <mutex>

#include <plugins/plugin_tools.h>
#include <nx/sdk/uuid.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/analytics/i_engine.h>

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
class Engine: public nxpt::CommonRefCounter<IEngine>
{
protected:
    LogUtils logUtils;

protected:
    /**
     * @param enableOutput Enables NX_OUTPUT. Typically, use NX_DEBUG_ENABLE_OUTPUT as a value.
     * @param printPrefix Prefix for NX_PRINT and NX_OUTPUT. If empty, will be made from libName.
     */
    Engine(
        IPlugin* plugin,
        bool enableOutput,
        const std::string& printPrefix = "");

    virtual std::string manifest() const = 0;

    /**
     * Called when the settings are received from the server (even if the values are not changed).
     * Should perform any required (re)initialization. Called even if the settings model is empty.
     */
    virtual void settingsReceived() {}

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
        Uuid /*objectId*/,
        Uuid /*deviceId*/,
        int64_t /*timestampUs*/,
        const std::map<std::string, std::string>& /*params*/,
        std::string* /*outActionUrl*/,
        std::string* /*outMessageToUser*/,
        Error* /*error*/)
    {
    }

    /**
     * Sends a PluginEvent to the Server. Can be called from any thread, but if called before
     * settingsReceived() was called, will be ignored in case setHandler() was not called yet.
     */
    void pushPluginEvent(
        nx::sdk::IPluginEvent::Level level,
        std::string caption,
        std::string description);

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

    IEngine::IHandler* handler() { return m_handler; }

public:
    virtual ~Engine() override;

    virtual IPlugin* plugin() const override { return m_plugin; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual void setSettings(const nx::sdk::IStringMap* settings) override;
    virtual nx::sdk::IStringMap* pluginSideSettings() const override;
    virtual const IString* manifest(Error* error) const override;

    virtual void executeAction(IAction* action, Error* outError) override;
    virtual nx::sdk::Error setHandler(IEngine::IHandler* handler) override;
    virtual bool isCompatible(const nx::sdk::DeviceInfo* deviceInfo) const override;

private:
    void assertPluginCasted(void* plugin) const;

private:
    mutable std::mutex m_mutex;
    IPlugin* const m_plugin;
    std::map<std::string, std::string> m_settings;
    IEngine::IHandler* m_handler = nullptr;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
