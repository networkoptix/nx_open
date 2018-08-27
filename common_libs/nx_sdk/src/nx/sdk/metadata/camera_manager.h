#pragma once

#include <plugins/plugin_api.h>

#include <nx/sdk/common.h>
#include <nx/sdk/metadata/metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

// TODO: Consider renaming and moving. Used by MetadataHandler::getParamValue().
static const int NX_NO_ERROR = 0;
static const int NX_UNKNOWN_PARAMETER = -41;
static const int NX_MORE_DATA = -23;

/**
 * Interface for handler that processes metadata incoming from the plugin.
 */
class MetadataHandler
{
public:
    virtual ~MetadataHandler() = default;

    /**
     * @param error Used for reporting errors to the outer code.
     * @param metadata Incoming from the plugin.
     */
    virtual void handleMetadata(Error error, MetadataPacket* metadata) = 0;
};

/**
 * Each class that implements CameraManager interface should properly handle this GUID in
 * its queryInterface method.
 */
static const nxpl::NX_GUID IID_CameraManager =
    {{0x48, 0x5a, 0x23, 0x51, 0x55, 0x73, 0x4f, 0xb5, 0xa9, 0x11, 0xe4, 0xfb, 0x22, 0x87, 0x79, 0x24}};

class Plugin;

/**
 * Interface used to control the process of fetching metadata from the resource.
 */
class CameraManager: public nxpl::PluginInterface
{
public:
    /**
     * Start fetching metadata from the resource.
     * @param eventTypeList pointer to Guid array.
     * @param eventTypeListSize guid array size.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error startFetchingMetadata(
        nxpl::NX_GUID* typeList, int typeListSize) = 0;

    /**
     * @param handler Processes event metadata and object metadata fetched by the plugin. The
     *     plugin will fetch events metadata after startFetchingMetadata() call. Errors should also
     *     be reported via this handler. Also provides other services to the manager, e.g.
     *     reading settings that are stored by the server.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error setHandler(MetadataHandler* handler) = 0;

    /**
     * Stops fetching metadata from the resource synchronously
     * @return noError in case of success, other value otherwise.
     */
    virtual Error stopFetchingMetadata() = 0;

    /**
     * Provides a 0-terminated UTF-8 string containing the JSON manifest.
     * @return Pointer to a C-style string which MUST be valid untill freeManifest() is invoked.
     * JSON manifest may have one of the two schemas. First contains only event guids, e.g.:
     * {
     *     "supportedEventTypes":
     *     [
     *         "{b37730fe-3e2d-9eb7-bee0-7732877ec61f}",
     *         "{f83daede-7fae-6a51-2e90-69017dadfd62}",
     *     ]
     * }
     *
     * The second contains guids and descriptions, e.g.:
     * {
     *     "outputEventTypes":
     *     [
     *         {
     *             "eventTypeId": "ae197d39-2fc5-d798-abe6-07329771417f",
     *             "eventName": { "value": "Create Recording", "localization": { } }
     *         },
     *         {
     *             "eventTypeId": "eae2bd46-1690-0c22-8096-53ed3f90ac14",
     *             "eventName": { "value": "Delete Recording", "localization": { } }
     *         }
     *     ]
     * }
     */
    virtual const char* capabilitiesManifest(Error* error) = 0;

    /**
     * Tells CameraManager that the memory previously returned by capabilitiesManifest() pointed to
     * by data is no longer needed and may be disposed.
     */
    virtual void freeManifest(const char* data) = 0;

    /**
     * Called before other methods. Server provides the set of settings stored in its database for
     * the combination of a resource instance and a plugin type.
     * @param settings Values of settings declared in the manifest. The pointer is valid only
     *     during the call. If count is 0, the pointer is null.
     */
    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
