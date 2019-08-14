// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

#include <nx/sdk/i_string_map.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/i_settings_response.h>

#include <nx/sdk/analytics/i_metadata_types.h>
#include <nx/sdk/analytics/i_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

class IEngine; //< Forward declaration for the parent object.

/**
 * Metadata producer for a particular Device (e.g. a camera). This is a base interface which allows
 * to identify the Device which the Engine has been assigned to work with, and is suitable only for
 * Plugins which generate metadata without additional input from the Device. If the Plugin requires
 * input data from the Device (e.g. video frames from the camera), it may implement one of the
 * derived interfaces that offer such data.
 *
 * All methods are guaranteed to be called without overlapping even if from different threads (i.e.
 * with a guaranteed barrier between the calls), thus, no synchronization is required for the
 * implementation.
 */
class IDeviceAgent: public Interface<IDeviceAgent>
{
public:
    static auto interfaceId() { return makeId("nx::sdk::analytics::IDeviceAgent"); }

    class IHandler: public Interface<IHandler>
    {
    public:
        static auto interfaceId() { return makeId("nx::sdk::analytics::IDeviceAgent::IHandler"); }

        virtual ~IHandler() = default;
        virtual void handleMetadata(IMetadataPacket* metadataPacket) = 0;
        virtual void handlePluginDiagnosticEvent(IPluginDiagnosticEvent* event) = 0;
    };

    /**
     * Called before other methods. Server provides the set of settings stored in its database,
     * combined with the values received from the plugin via pluginSideSettings() (if any), for
     * the combination of a device instance and an Engine instance.
     *
     * @param settings Values of settings declared in the manifest. Never null.
     * @return Result containing a map of errors that occurred while applying each setting - the
     *     keys are the setting ids, and the values are human readable error strings in English.
     *     Even if some settings can't be applied or an error happens while applying them, this
     *     method must return a successful result with a corresponding map of errors. A faulty
     *     result containing error information instead of the map should be returned only in case
     *     of some general failure that affected the procedure of applying the settings. The result
     *     should contain null if no errors occurred.
     */
    protected: virtual void doSetSettings(
        Result<const IStringMap*>* outResult, const IStringMap* settings) = 0;
    public: Result<const IStringMap*> setSettings(const IStringMap* settings)
    {
        Result<const IStringMap*> result;
        doSetSettings(&result, settings);
        return result;
    }

    /**
     * In addition to the settings stored in a Server database, a DeviceAgent can have some
     * settings which are stored somewhere "under the hood" of the DeviceAgent, e.g. on a device
     * acting as a DeviceAgent's backend. Such settings do not need to be explicitly marked in the
     * Settings Model, but every time the Server offers the user to edit the values, it calls this
     * method and merges the received values with the ones in its database.
     *
     * @return Result containing (in case of success) information about settings that are stored on
     *     the plugin side. Errors corresponding to particular settings should be placed in the
     *     ISettingsResponse object. A faulty result must be returned only in case of a general
     *     failure that affects the settings retrieval procedure. The result should contain null if
     *     the Engine has no plugin-side settings.
     */
    protected: virtual void getPluginSideSettings(
        Result<const ISettingsResponse*>* outResult) const = 0;
    public: Result<const ISettingsResponse*> pluginSideSettings() const
    {
        Result<const ISettingsResponse*> result;
        getPluginSideSettings(&result);
        return result;
    }

    /**
     * Provides DeviceAgent manifest in JSON format.
     *
     * @return JSON string in UTF-8.
     */
    protected: virtual void getManifest(Result<const IString*>* outResult) const = 0;
    public: Result<const IString*> manifest() const
    {
        Result<const IString*> result;
        getManifest(&result);
        return result;
    }

    /**
     * @param handler Processes event metadata and object metadata fetched by DeviceAgent.
     *     DeviceAgent should fetch events metadata after setNeededMetadataTypes() call.
     *     Generic device related events (errors, warning, info messages) might also be reported
     *     via this handler.
     */
    virtual void setHandler(IHandler* handler) = 0;

    /**
     * Sets a list of metadata types that are needed by the Server. Empty list means that the
     * Server does not need any metadata from this DeviceAgent.
     *
     * @param neededMetadataTypes Lists of type ids of events and objects.
     */
    protected: virtual void doSetNeededMetadataTypes(
        Result<void>* outResult, const IMetadataTypes* neededMetadataTypes) = 0;
    public: Result<void> setNeededMetadataTypes(const IMetadataTypes* neededMetadataTypes)
    {
        Result<void> result;
        doSetNeededMetadataTypes(&result, neededMetadataTypes);
        return result;
    }
};

} // namespace analytics
} // namespace sdk
} // namespace nx
