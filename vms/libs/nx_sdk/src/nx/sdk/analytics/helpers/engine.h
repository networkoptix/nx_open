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
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/log_utils.h>

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
    Engine(bool enableOutput, const std::string& printPrefix = "");

    virtual std::string manifestString() const = 0;

    /**
     * Called when the settings are received from the Server, even if the values are not changed.
     * Should perform any required (re)initialization. Called even if the settings model is empty.
     * @return Error messages per setting (if any), as in IEngine::setSettings().
     */
    virtual Result<const IStringMap*> settingsReceived() { return nullptr; }

    /**
     * Provides access to the Engine global settings stored by the Server.
     *
     * ATTENTION: If settingsReceived() has not been called yet, it means that the Engine has not
     * received its settings from the Server yet, and thus this method will yield empty values.
     *
     * @return Setting value, or an empty string if such setting does not exist, having logged the
     *     error.
     */
    std::string settingValue(const std::string& settingName);

    /**
     * Action handler. Called when some action defined by this engine is triggered by Server.
     * @param actionId Id of the action being triggered.
     * @param objectTrackId Id of an object track for which the action has been triggered.
     * @param params If the engine manifest defines params for the action being triggered,
     *     contains their values after they are filled by the user via Client form. Otherwise,
     *     empty.
     * @param outActionUrl If set by this call, Client will open this URL in an embedded browser.
     * @param outMessageToUser If set by this call, Client will show this text to the user.
     */
    virtual Result<void> executeAction(
        const std::string& /*actionId*/,
        Uuid /*objectTrackId*/,
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

    IHandler* handler() const { return m_handler.get(); }

public:
    virtual ~Engine() override;

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void setEngineInfo(const IEngineInfo* engineInfo) override;
    virtual void setHandler(IEngine::IHandler* handler) override;
    virtual bool isCompatible(const IDeviceInfo* deviceInfo) const override;

protected:
    virtual void doSetSettings(
        Result<const IStringMap*>* outResult, const IStringMap* settings) override;
    virtual void getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const override;
    virtual void getManifest(Result<const IString*>* outResult) const override;
    virtual void doExecuteAction(Result<void>* outResult, IAction* action) override;

private:
    mutable std::mutex m_mutex;
    const std::string m_overridingPrintPrefix;
    std::map<std::string, std::string> m_settings;
    Ptr<IEngine::IHandler> m_handler;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
