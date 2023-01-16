// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/i_active_setting_changed_action.h>
#include <nx/sdk/i_active_setting_changed_response.h>
#include <nx/sdk/i_device_info.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

#include "i_action.h"
#include "i_device_agent.h"
#include "i_engine_info.h"

namespace nx::sdk::analytics {

class IPlugin; //< Forward declaration for the parent object.

class IEngine0: public Interface<IEngine0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IEngine"); }

    class IHandler: public Interface<IHandler>
    {
    public:
        static auto interfaceId() { return makeId("nx::sdk::analytics::IEngine::IHandler"); }

        virtual ~IHandler() = default;
        virtual void handlePluginDiagnosticEvent(IPluginDiagnosticEvent* event) = 0;
    };

    /**
     * Called right after the Engine creation (before all other methods) or when some
     * Engine-related change occurs on the Server side (e.g. Engine name is changed).
     */
    virtual void setEngineInfo(const IEngineInfo* engineInfo) = 0;

    /** Called by setSettings() */
    protected: virtual void doSetSettings(
        Result<const ISettingsResponse*>* outResult, const IStringMap* settings) = 0;
    /**
     * Receives the values of settings stored in the Server database for this Engine instance. The
     * values must conform to the Settings Model, which is initially defined in the Plugin
     * Manifest, but may be overridden via returning an updated Model from this method.
     *
     * After creating the new IEngine instance, this method is called after setEngineInfo() but
     * before manifest().
     *
     * @param settings Values of settings conforming to the Settings Model. Never null.
     *
     * @return An error code with a message in case of some general failure that affected the
     *     procedure of applying the settings, or a combination of optional individual setting
     *     errors, optional new setting values in case they were changed during the applying, and
     *     an optional new Settings Model. Can be null if none of the above items are present.
     */
    public: Result<const ISettingsResponse*> setSettings(const IStringMap* settings)
    {
        Result<const ISettingsResponse*> result;
        doSetSettings(&result, settings);
        return result;
    }

    /** Called by pluginSideSettings() */
    protected: virtual void getPluginSideSettings(
        Result<const ISettingsResponse*>* outResult) const = 0;
    /**
     * In addition to the settings stored in the Server database, an Engine can have some settings
     * which are stored somewhere "under the hood" of the Engine, e.g. on a device acting as an
     * Engine's backend. Such settings do not need to be explicitly marked in the Settings Model,
     * but every time the Server offers the user to edit the values, it calls this method and
     * merges the received values with the ones in its database.
     *
     * @return An error code with a message in case of some general failure, or a combination of
     *     optional individual setting errors, optional setting values, and an optional new
     *     Settings Model. Can be null if none of the above items are present.
     */
    public: Result<const ISettingsResponse*> pluginSideSettings() const
    {
        Result<const ISettingsResponse*> result;
        getPluginSideSettings(&result);
        return result;
    }

    /** Called by manifest() */
    protected: virtual void getManifest(Result<const IString*>* outResult) const = 0;
    /**
     * Provides a JSON Manifest for this Engine instance. See the example of such Manifest in
     * stub_analytics_plugin.
     *
     * When creating the new IEngine instance, this method is called after setSettings(), but the
     * Manifest must not depend on the setting values, because it will not be re-requested on
     * settings change afterwards.
     *
     * See the manifest specification in @ref md_src_nx_sdk_analytics_manifests.
     *
     * @return JSON string in UTF-8.
     */
    public: Result<const IString*> manifest() const
    {
        Result<const IString*> result;
        getManifest(&result);
        return result;
    }

    /**
     * @return True if the Engine is able to create DeviceAgents for the provided device, false
     *     otherwise.
     */
    virtual bool isCompatible(const IDeviceInfo* deviceInfo) const = 0;

    /** Called by obtainDeviceAgent() */
    protected: virtual void doObtainDeviceAgent(
        Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo) = 0;
    /**
     * Creates, or returns an already existing, a DeviceAgent instance intended to work with the
     * given device.
     *
     * @param deviceInfo Information about the device for which a DeviceAgent should be created.
     * @return Pointer to an object that implements IDeviceAgent interface, or null if a
     *     DeviceAgent for this particular Device makes no sense (e.g. if the Device supports no
     *     Analytics Events and Objects) - this is not treated as an error (the user is not
     *     notified).
     */
    public: Result<IDeviceAgent*> obtainDeviceAgent(const IDeviceInfo* deviceInfo)
    {
        Result<IDeviceAgent*> result;
        doObtainDeviceAgent(&result, deviceInfo);
        return result;
    }

    /** Called by executeAction() */
    protected: virtual void doExecuteAction(
        Result<IAction::Result>* outResult, const IAction* action) = 0;
    /**
     * Action handler. Called when some action defined by this Engine is triggered by Server.
     *
     * @param action Provides data for the action such as metadata object for which the action has
     *     been triggered, and a means for reporting back action results to Server. This object
     *     should not be used after returning from this function.
     */
    public: Result<IAction::Result> executeAction(const IAction* action)
    {
        Result<IAction::Result> result;
        doExecuteAction(&result, action);
        return result;
    }

    /**
     * @param handler Generic Engine-related events (errors, warning, info messages)
     *     might be reported via this handler.
     */
    virtual void setHandler(IHandler* handler) = 0;
};

/**
 * Main interface for an Analytics Engine instance. The instances are created by a Mediaserver via
 * calling analytics::IPlugin::createEngine() typically on Mediaserver start (or when a new Engine
 * is created by the system administrator), and destroyed (via releaseRef()) on Mediaserver
 * shutdown (or when an existing Engine is deleted by the system administrator).
 *
 * For the VMS end user, each Engine instance is perceived as an independent Analytics Engine
 * which has its own set of values of settings stored in the Mediaserver database.
 *
 * All methods are guaranteed to be called without overlapping even if from different threads (i.e.
 * with a guaranteed barrier between the calls), thus, no synchronization is required for the
 * implementation.
 */
class IEngine: public Interface<IEngine, IEngine0>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IEngine1"); }

    /** Called by getSettingsOnActiveSettingChange() */
    protected: virtual void doGetSettingsOnActiveSettingChange(
        Result<const IActiveSettingChangedResponse*>* outResult,
        const IActiveSettingChangedAction* activeSettingChangedAction) = 0;
    /**
     * When a setting marked as Active changes its value in the GUI, the Server calls this method
     * to notify the Plugin and allow it to adjust the values of the settings and the Settings
     * Model. This mechanism allows certain settings to depend on the current values of others,
     * for example, switching a checkbox or a drop-down can lead to some other setting being
     * replaced with another, or some values being converted to a different measurement unit.
     *
     * @return An error code with a message in case of some general failure that affected the
     *     procedure of analyzing the settings, or data defining the interaction with the user and
     *     the new state of the settings dialog. Can be null if no user interaction is needed.
     */
    public: Result<const IActiveSettingChangedResponse*> getSettingsOnActiveSettingChange(
        const IActiveSettingChangedAction* activeSettingChangedAction)
    {
        Result<const IActiveSettingChangedResponse*> result;
        doGetSettingsOnActiveSettingChange(&result, activeSettingChangedAction);
        return result;
    }
};
using IEngine1 = IEngine;

} // namespace nx::sdk::analytics
