#pragma once

#include <plugins/plugin_api.h>

#include <nx/sdk/common.h>
#include <nx/sdk/analytics/metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

// TODO: Consider renaming and moving. Used by MetadataHandler::getParamValue().
static const int NX_NO_ERROR = 0;
static const int NX_UNKNOWN_PARAMETER = -41;
static const int NX_MORE_DATA = -23;

/**
 * Interface for handler that processes metadata incoming from the engine.
 */
class MetadataHandler
{
public:
    virtual ~MetadataHandler() = default;

    /**
     * @param error Used for reporting errors to the outer code.
     * @param metadata Incoming from the engine.
     */
    virtual void handleMetadata(Error error, MetadataPacket* metadata) = 0;
};

/**
 * Each class that implements DeviceAgent interface should properly handle this GUID in
 * its queryInterface method.
 */
static const nxpl::NX_GUID IID_DeviceAgent =
    {{0x48, 0x5a, 0x23, 0x51, 0x55, 0x73, 0x4f, 0xb5, 0xa9, 0x11, 0xe4, 0xfb, 0x22, 0x87, 0x79, 0x24}};

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
    /** @return Parent Engine. */
    virtual Engine* engine() const = 0;

    /**
     * Starts fetching metadata from the resource.
     * @param typeList List of types of events and objects as an array of C-style strings; null if
     *     the array is empty.
     * @param typeListSize Number of items in typeList array.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error startFetchingMetadata(
        const char* const* typeList, int typeListSize) = 0;

    /**
     * @param handler Processes event metadata and object metadata fetched by the engine. The
     *     engine will fetch events metadata after startFetchingMetadata() call. Errors should also
     *     be reported via this handler. Also provides other services to the DeviceAgent, e.g.
     *     reading settings that are stored by the Server.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error setMetadataHandler(MetadataHandler* handler) = 0;

    /**
     * Stops fetching metadata from the resource.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error stopFetchingMetadata() = 0;

    /**
     * Provides a 0-terminated UTF-8 string containing the JSON manifest.
     * @return Pointer to a C-style string which MUST be valid untill freeManifest() is invoked.
     * JSON manifest may have one of the two schemas. First contains only event type ids, e.g.:
     * {
     *     "supportedEventTypes":
     *     [
     *         "nx.lineCrossing",
     *         "nx.faceDetected",
     *     ]
     * }
     *
     * The second contains ids and descriptions, e.g.:
     * {
     *     "outputEventTypes":
     *     [
     *         {
     *             "id": "nx.lineCrossing",
     *             "name": { "value": "Line crossed", "localization": {} }
     *         },
     *         {
     *             "id": "nx.faceDetected",
     *             "name": { "value": "Face detected", "localization": {} }
     *         }
     *     ]
     * }
     */
    virtual const char* manifest(Error* error) = 0;

    /**
     * Tells DeviceAgent that the memory previously returned by manifest() pointed to
     * by data is no longer needed and may be disposed.
     */
    virtual void freeManifest(const char* data) = 0;

    /**
     * Called before other methods. Server provides the set of settings stored in its database for
     * the combination of a resource instance and a engine type.
     * @param settings Values of settings declared in the manifest. The pointer is valid only
     *     during the call. If count is 0, the pointer is null.
     */
    virtual void setSettings(const nxpl::Setting* settings, int count) = 0;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
