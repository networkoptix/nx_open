#pragma once

#include <plugins/plugin_api.h>

#include <plugins/metadata/abstract_data_packet.h>
#include <plugins/metadata/abstract_metadata_packet.h>
#include <plugins/metadata/utils.h>

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
    /**
     * @param error used for reporting errors to the outer code.
     * @param metadata incoming from the plugin
     */
    virtual void handleMetadata(
        Error error,
        const AbstractMetadataPacket** outMetadata) = 0;
};

/**
 * Each class that implements AbstractMediaFrame interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_AbstractMetadataManager =
    {{0x48, 0x5a, 0x23, 0x51, 0x55, 0x73, 0x4f, 0xb5, 0xa9, 0x11, 0xe4, 0xfb, 0x22, 0x87, 0x79, 0x24}};

/**
 * @brief The AbstractMetadataManager interface is used to control
 * process of fetching metadata from the resource
 */
class AbstractMetadataManager: nxpl::PluginInterface
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
     * @param buffer buffer for the manifest
     * @param inOutBufferSize size of the manifest buffer. If buffer is too small
     * then this method MUST return needMoreBufferSpace and set inOutBufferSize
     * to the desired value.
     * @return noError in case of success,
     * needMoreBufferSpace in case buffer size is not enough.
     * some other value otherwise.
     */
    virtual const char* capabiltiesManifest() = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
