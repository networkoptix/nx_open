#pragma once

#include <plugins/plugin_api.h>

#include <nx/sdk/common.h>
#include <nx/sdk/metadata/abstract_data_packet.h>
#include <nx/sdk/metadata/abstract_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Interface for handler that processes metadata incoming from the plugin.
 */
class AbstractMetadataHandler
{
public:
    virtual ~AbstractMetadataHandler() = default;

    /**
     * @param error Used for reporting errors to the outer code.
     * @param metadata Incoming from the plugin.
     */
    virtual void handleMetadata(
        Error error,
        AbstractMetadataPacket* metadata) = 0;
};

/**
 * Each class that implements AbstractMetadataManager interface should properly handle this GUID in
 * its queryInterface method.
 */
static const nxpl::NX_GUID IID_MetadataManager =
    {{0x48, 0x5a, 0x23, 0x51, 0x55, 0x73, 0x4f, 0xb5, 0xa9, 0x11, 0xe4, 0xfb, 0x22, 0x87, 0x79, 0x24}};

/**
 * Interface used to control the process of fetching metadata from the resource.
 */
class AbstractMetadataManager: public nxpl::PluginInterface
{
public:
    // TODO: #mike: Decide on separation between events and other metadata packet types.

    /**
     * Starts fetching metadata from the resource.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error startFetchingMetadata() = 0;

    /**
     * Starts fetching metadata from the resource.
     * @param handler Processes event metadata and object metadata fetched by the plugin. The
     *     plugin will fetch events metadata after startFetchingMetadata() call. Errors should also
     *     be reported via this handler.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error setHandler(AbstractMetadataHandler* handler) = 0;

    /**
     * Stops fetching metadata from the resource synchronously
     * @return noError in case of success, other value otherwise.
     */
    virtual Error stopFetchingMetadata() = 0;

    /**
     * Provides null terminated UTF8 string containing json manifest according to
     * nx_metadata_plugin_manifest.schema.json.
     * @return Pointer to c-style string which MUST be valid while this Manager instance exists.
     */
    virtual const char* capabilitiesManifest(Error* error) const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
