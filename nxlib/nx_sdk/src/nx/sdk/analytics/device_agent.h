#pragma once

#include <plugins/plugin_api.h>

#include <nx/sdk/common.h>
#include <nx/sdk/settings.h>
#include <nx/sdk/i_string.h>
#include <nx/sdk/i_plugin_event.h>
#include <nx/sdk/analytics/metadata_types.h>
#include <nx/sdk/analytics/metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

// TODO: Consider renaming and moving. Used by MetadataHandler::getParamValue().
static const int NX_NO_ERROR = 0;
static const int NX_UNKNOWN_PARAMETER = -41;
static const int NX_MORE_DATA = -23;

/**
 * Each class that implements DeviceAgent interface should properly handle this GUID in
 * its queryInterface method.
 */
static const nxpl::NX_GUID IID_DeviceAgent =
    {{0x48,0x5a,0x23,0x51,0x55,0x73,0x4f,0xb5,0xa9,0x11,0xe4,0xfb,0x22,0x87,0x79,0x24}};

class Engine; //< Forward declaration for the parent object.

/**
 * Interface used to control the process of fetching metadata from the resource.
 *
 * All methods are guaranteed to be called without overlappings, even if from different threads,
 * thus, no synchronization is required for the implementation.
 */
class DeviceAgent: public nxpl::PluginInterface
{
public:
    class IHandler
    {
    public:
        virtual ~IHandler() = default;
        virtual void handleMetadata(MetadataPacket* metadataPacket) = 0;
        virtual void handlePluginEvent(IPluginEvent* event) = 0;
    };

    /** @return Parent Engine. */
    virtual Engine* engine() const = 0;

    /**
     * Called before other methods. Server provides the set of settings stored in its database for
     * the combination of a resource instance and a engine type.
     * @param settings Values of settings declared in the manifest.
     */
    virtual void setSettings(const Settings* settings) = 0;

    /**
     * In addition to the settings stored in a Server database, a DeviceAgent can have some
     * settings which are stored somewhere "under the hood" of the Engine, e.g. on a device acting
     * as a DeviceAgent's backend. Such settings do not need to be explicitly marked in the Settings
     * Model, but every time the Server offers the user to edit the values, it calls this method and
     * merges the received values with the ones in its database.
     * 
     * @return DeviceAgent settings that are stored on the plugin side.
     */
    virtual Settings* pluginSideSettings() const = 0;

    /**
     * Provides DeviceAgent manifest in JSON format.
     * @return JSON string in UTF-8.
     */
    virtual const IString* manifest(Error* error) const = 0;

    /**
     * @param handler Processes event metadata and object metadata fetched by DeviceAgent.
     *  DeviceAgent should fetch events metadata after setNeededMetadataTypes() call.
     *  Generic device related events (errors, warning, info messages) might also be reported
     *  via this handler.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error setHandler(IHandler* handler) = 0;

    /**
     * Sets a list of metadata types that are needed by the Server. Empty list means that the
     * Server doesn't need any metadata from this DeviceAgent.
     * @param neededMetadataTypes Lists of type ids of events and objects.
     */
    virtual Error setNeededMetadataTypes(const IMetadataTypes* neededMetadataTypes) = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
