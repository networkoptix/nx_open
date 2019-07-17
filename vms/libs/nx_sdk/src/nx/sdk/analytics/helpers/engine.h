// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <map>
#include <mutex>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/analytics/i_engine.h>

#include <nx/sdk/result.h>
#include <nx/sdk/uuid.h>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/helpers/result_aliases.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Base class for a typical implementation of an Analytics Engine. Hides many technical details of
 * the Analytics Plugin SDK, but may limit plugin capabilities - use only when suitable.
 *
 * To use NX_PRINT/NX_OUTPUT in a derived class with the prefix defined by this class, add the
 * following to the derived class .cpp:
 * <pre><code>
 *     #define NX_PRINT_PREFIX (this->logUtils.printPrefix)
 *     #include <nx/kit/debug.h>
 * </code></pre>
 */
class Engine: public RefCountable<IEngine>
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

    virtual std::string manifestString() const = 0;

    /**
     * Called when the settings are received from the Server, even if the values are not changed.
     * Should perform any required (re)initialization. Called even if the settings model is empty.
     * @return Error messages per setting (if any), as in IEngine::setSettings().
     */
    virtual StringMapResult settingsReceived() { return nullptr; }

    /**
     * Provides access to the Plugin global settings stored by the server.
     * @return Param value, or an empty string if such param does not exist, having logged the
     * error.
     */
    std::string getParamValue(const std::string& paramName);

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
    virtual Result<void> executeAction(
        const std::string& /*actionId*/,
        Uuid /*objectId*/,
        Uuid /*deviceId*/,
        int64_t /*timestampUs*/,
        Ptr<IObjectTrackInfo> /*trackInfo*/,
        const std::map<std::string, std::string>& /*params*/,
        std::string* /*outActionUrl*/,
        std::string* /*outMessageToUser*/)
    {
        return {};
    }

    /**
     * Sends a PluginDiagnosticEvent to the Server. Can be called from any thread, but if called
     * before settingsReceived() was called, will be ignored in case setHandler() was not called
     * yet.
     */
    void pushPluginDiagnosticEvent(
        IPluginDiagnosticEvent::Level level,
        std::string caption,
        std::string description);

    /**
     * Intended to be called from a method of a derived class overriding plugin().
     * @return Parent Plugin, casted to the specified type.
     */
    template<typename DerivedPlugin>
    DerivedPlugin* pluginCasted() const
    {
        const auto plugin = dynamic_cast<DerivedPlugin*>(m_plugin);
        assertPluginCasted(plugin);
        return plugin;
    }

    IHandler* handler() const { return m_handler.get(); }

public:
    virtual ~Engine() override;

    virtual IPlugin* plugin() const override { return m_plugin; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void setEngineInfo(const IEngineInfo* engineInfo) override;
    virtual StringMapResult setSettings(const IStringMap* settings) override;
    virtual SettingsResponseResult pluginSideSettings() const override;
    virtual StringResult manifest() const override;
    virtual Result<void> executeAction(IAction* action) override;
    virtual void setHandler(IEngine::IHandler* handler) override;
    virtual bool isCompatible(const IDeviceInfo* deviceInfo) const override;

private:
    void assertPluginCasted(void* plugin) const;

private:
    mutable std::mutex m_mutex;
    IPlugin* const m_plugin;
    const std::string m_overridingPrintPrefix;
    std::map<std::string, std::string> m_settings;
    Ptr<IEngine::IHandler> m_handler;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
