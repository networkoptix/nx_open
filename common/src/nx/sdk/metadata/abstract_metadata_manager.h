#pragma once

#include <plugins/plugin_api.h>

#include <nx/sdk/common.h>
#include <nx/sdk/metadata/abstract_data_packet.h>
#include <nx/sdk/metadata/abstract_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * @brief The AbstractMetadataHandler class is an interface for handler
 * that processes metadata incoming from the plugin.
 */
class AbstractMetadataHandler
{
public:
    virtual ~AbstractMetadataHandler() = default;

    /**
     * @param error used for reporting errors to the outer code.
     * @param metadata incoming from the plugin
     */
    virtual void handleMetadata(
        Error error,
        AbstractMetadataPacket* metadata) = 0;
};

/**
 * Each class that implements AbstractMetadataManager interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_MetadataManager =
    {{0x48, 0x5a, 0x23, 0x51, 0x55, 0x73, 0x4f, 0xb5, 0xa9, 0x11, 0xe4, 0xfb, 0x22, 0x87, 0x79, 0x24}};

/**
 * @brief The AbstractMetadataManager interface is used to control
 * process of fetching metadata from the resource
 */
class AbstractMetadataManager: public nxpl::PluginInterface
{
public:

    /**
     * @brief startFetchingMetadata starts fetching metadata from the resource.
     * @param handler metadata fetched by plugin should be passed to this handler.
     * Errors are also should reported via this handler.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error startFetchingMetadata(AbstractMetadataHandler* handler) = 0;

    /**
     * @brief stopFetchingMetadata stops fetching metadata from the resource synchronously
     * @return noError in case of success, other value otherwise.
     */
    virtual Error stopFetchingMetadata() = 0;

    /**
     * @brief provides null terminated UTF8 string containing json manifest
     * according to nx_metadata_plugin_manifest.schema.json.
     * @return pointer to c-style string which MUST be valid till manager object exists
     */
    virtual const char* capabilitiesManifest(Error* error) const = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
