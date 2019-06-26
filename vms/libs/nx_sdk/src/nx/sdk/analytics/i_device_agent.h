// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/sdk/interface.h>
#include <nx/sdk/i_error.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/analytics/i_metadata_types.h>
#include <nx/sdk/analytics/i_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

class IEngine; //< Forward declaration for the parent object.

/**
 * Used to control the process of fetching metadata from the resource.
 *
 * All methods are guaranteed to be called without overlappings, even if from different threads,
 * thus, no synchronization is required for the implementation.
 */
class IDeviceAgent: public Interface<IDeviceAgent>
{
public:
    static auto interfaceId() { return InterfaceId("nx::sdk::analytics::IDeviceAgent"); }

    class IHandler: public Interface<IHandler>
    {
    public:
        static auto interfaceId()
        {
            return InterfaceId("nx::sdk::analytics::IDeviceAgent::IHandler");
        }

        virtual ~IHandler() = default;
        virtual void handleMetadata(IMetadataPacket* metadataPacket) = 0;
        virtual void handlePluginDiagnosticEvent(IPluginDiagnosticEvent* event) = 0;
    };

    /** @return Parent Engine. */
    virtual IEngine* engine() const = 0;

    /**
     * Called before other methods. Server provides the set of settings stored in its database,
     * combined with the values received from the plugin via pluginSideSettings() (if any), for
     * the combination of a device instance and an Engine instance.
     *
     * @param outError Output parameter for error reporting. Never null. Must contain
     *     `ErrorCode::noError` error code in the case of success (`outError` object is guarnteed to
     *     be prefilled with `noError` value, so no additional actions are required) or be properly
     *     filled in a case of failure. `setSetting()` method should set information about each
     *     failed setting via `IError::setDetail()` method (the key is a setting id and the value is
     *     an error message).
     * @param settings Values of settings declared in the manifest. Never null. Valid only during
     *     the call.
     */
    virtual void setSettings(const IStringMap* settings, IError* outError) = 0;

    /**
     * In addition to the settings stored in a Server database, a DeviceAgent can have some
     * settings which are stored somewhere "under the hood" of the DeviceAgent, e.g. on a device
     * acting as a DeviceAgent's backend. Such settings do not need to be explicitly marked in the
     * Settings Model, but every time the Server offers the user to edit the values, it calls this
     * method and merges the received values with the ones in its database.
     *
     * @param outError Output parameter for error reporting. Never null. Must contain
     *     `ErrorCode::noError` error code in the case of success (`outError` object is guarnteed to
     *     be prefilled with `noError` value, so no additional actions are required) or be properly
     *     filled in a case of failure. The `pluginSideSettings()` method should set information
     *     about each setting it failed to retrieve via `IError::setDetail()` method (the key is a
     *     setting id and the value is an error message).
     * @return DeviceAgent settings that are stored on the plugin side, or null if there are no
     *     such settings.
     */
    virtual IStringMap* pluginSideSettings(IError* outError) const = 0;

    /**
     * Provides DeviceAgent manifest in JSON format.
     * @param outError Output parameter for error reporting. Never null. Must contain
     *     `ErrorCode::noError` error code in the case of success (`outError` object is guarnteed to
     *     be prefilled with `noError` value, so no additional actions are required) or be properly
     *     filled in a case of failure.
     * @return JSON string in UTF-8.
     */
    virtual const IString* manifest(IError* outError) const = 0;

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
     * @param neededMetadataTypes Lists of type ids of events and objects.
     * @param outError Output parameter for error reporting. Never null. Must contain
     *     `ErrorCode::noError` error code in the case of success (`outError` object is guarnteed to
     *     be prefilled with `noError` value, so no additional actions are required) or be properly
     *     filled in a case of failure.
     */
    virtual void setNeededMetadataTypes(
        const IMetadataTypes* neededMetadataTypes,
        IError* outError) = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
