// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/analytics/i_metadata_packet.h>
#include <nx/sdk/analytics/i_metadata_types.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/i_settings_response.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/interface.h>
#include <nx/sdk/result.h>

namespace nx::sdk::analytics {

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

    class IHandler0: public Interface<IHandler0>
    {
    public:
        static auto interfaceId() { return makeId("nx::sdk::analytics::IDeviceAgent::IHandler"); }

        virtual ~IHandler0() = default;

        /**
         * Passes a metadata packet to the Server. It's worth to mention that passing a single
         * metadata packet containing multiple metadata items with the same timestamp is preferred
         * to passing multiple metadata packets with the same timestamp containing a single item.
         * Although the latter will work correctly, it reduces the performance of the VMS Client.
         * For example, if you have 10 objects detected on the same frame, it's better to send 1
         * packet with all the object metadata than 10 separate packets.
         */
        virtual void handleMetadata(IMetadataPacket* metadataPacket) = 0;

        virtual void handlePluginDiagnosticEvent(IPluginDiagnosticEvent* event) = 0;
    };

    /**
     * Callback methods which allow to pass data from the Plugin to the Server. The methods can be
     * called from any thread at any moment.
     */
    class IHandler: public Interface<IHandler, IHandler0>
    {
    public:
        static auto interfaceId() { return makeId("nx::sdk::analytics::IDeviceAgent::IHandler1"); }

        /** Must be called when the Plugin needs to change the data in the DeviceAgent manifest. */
        virtual void pushManifest(const IString* manifest) = 0;
    };

    /** Called by setSettings() */
    protected: virtual void doSetSettings(
        Result<const ISettingsResponse*>* outResult, const IStringMap* settings) = 0;
    /**
     * Receives the values of settings stored in the Server database for the combination of a
     * device instance and an Engine instance.
     *
     * After creating the new IDeviceAgent instance, this method is called after manifest().
     *
     * ATTENTION: If the DeviceAgent has some plugin-side settings (see pluginSideSettings()) which
     * are hosted in the backend (e.g. on the Device) and potentially can be changed by the user
     * directly on that backend, their values must be ignored in this function when it is called
     * for the first time after the creation of the DeviceAgent. Otherwise, these values, which
     * technically are the values last known to the Server, may override the values stored in the
     * backend and potentially changed by the user directly during the period when the DeviceAgent
     * did not exist (i.e., Analytics has been turned off for this Device).
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
     * In addition to the settings stored in a Server database, a DeviceAgent can have some
     * settings which are stored somewhere "under the hood" of the DeviceAgent, e.g. on a device
     * acting as a DeviceAgent's backend. Such settings do not need to be explicitly marked in the
     * Settings Model, but every time the Server offers the user to edit the values, it calls this
     * method and merges the received values with the ones in its database.
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
     * Provides DeviceAgent Manifest in JSON format.
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
     * @param handler Processes event metadata and object metadata fetched by DeviceAgent.
     *     DeviceAgent should fetch events metadata after setNeededMetadataTypes() call.
     *     Generic device related events (errors, warning, info messages) might also be reported
     *     via this handler.
     */
    virtual void setHandler(IHandler* handler) = 0;

    /** Called by setNeededMetadataTypes() */
    protected: virtual void doSetNeededMetadataTypes(
        Result<void>* outResult, const IMetadataTypes* neededMetadataTypes) = 0;
    /**
     * Sets a list of metadata types that are needed by the Server. Empty list means that the
     * Server does not need any metadata from this DeviceAgent.
     *
     * @param neededMetadataTypes Lists of type ids of events and objects.
     */
    public: Result<void> setNeededMetadataTypes(const IMetadataTypes* neededMetadataTypes)
    {
        Result<void> result;
        doSetNeededMetadataTypes(&result, neededMetadataTypes);
        return result;
    }
};
using IDeviceAgent0 = IDeviceAgent;

} // namespace nx::sdk::analytics
