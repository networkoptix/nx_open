#pragma once

#include <plugins/plugin_api.h>

#include <nx/sdk/common.h>
#include <nx/sdk/metadata/abstract_data_packet.h>
#include <nx/sdk/metadata/abstract_metadata_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

// TODO: Consider renaming and moving. Used by AbstractMetadataHandler::getParamValue().
static const int NX_NO_ERROR = 0;
static const int NX_UNKNOWN_PARAMETER = -41;
static const int NX_MORE_DATA = -23;

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

    /**
     * Reads a parameter value from the manager settings stored by the server.
     * @param paramName Unique id of the parameter as a nul-terminated utf-8 string.
     * @param valueBufSize In-out parameter. Should be set by the caller to the size available in
     *     the buffer including nul-terminator, and if this size is not enough, will be set to the
     *     size required to accomodate the string.
     * @return NX_NO_ERROR on success, NX_UNKNOWN_PARAMETER if paramName is unknown, or
     *     NX_MORE_DATA if *valueBufSize is too small.
     */
    virtual int getParamValue(const char* paramName, char* valueBuf, int* valueBufSize) const = 0;
};

/**
 * Each class that implements AbstractMetadataManager interface should properly handle this GUID in
 * its queryInterface method.
 */
static const nxpl::NX_GUID IID_MetadataManager =
    {{0x48, 0x5a, 0x23, 0x51, 0x55, 0x73, 0x4f, 0xb5, 0xa9, 0x11, 0xe4, 0xfb, 0x22, 0x87, 0x79, 0x24}};

class AbstractMetadataPlugin;

/**
 * Interface used to control the process of fetching metadata from the resource.
 */
class AbstractMetadataManager: public nxpl::PluginInterface
{
public:
    /**
     * Start fetching metadata from the resource.
     * @param eventTypeList pointer to Guid array.
     * @param eventTypeListSize guid array size.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error startFetchingMetadata(
        nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize) = 0;

    /**
     * @param handler Processes event metadata and object metadata fetched by the plugin. The
     *     plugin will fetch events metadata after startFetchingMetadata() call. Errors should also
     *     be reported via this handler. Also provides other services to the manager, e.g.
     *     reading settings that are stored by the server.
     * @return noError in case of success, other value otherwise.
     */
    virtual Error setHandler(AbstractMetadataHandler* handler) = 0;

    /**
     * Stops fetching metadata from the resource synchronously
     * @return noError in case of success, other value otherwise.
     */
    virtual Error stopFetchingMetadata() = 0;

    /**
     * @brief provides null terminated UTF8 string containing json manifest
     * @return pointer to c-style string which MUST be valid till method "freeManifest"
     * will be invoked
     * Json manifest may have one of two schemas.
     * First contains only event guids. Valid example:
     * {
     *     "supportedEventTypes":
	 *     [
	 *         "{b37730fe-3e2d-9eb7-bee0-7732877ec61f}",
	 *         "{f83daede-7fae-6a51-2e90-69017dadfd62}",
     *     ]
     * }
     * Second contains guids and descriptions. Valid example:
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
     * @brief tells manager that memory (previously returned by "capabilitiesManifest") pointed to
     * by "data" is no longer needed and may be disposed
     */
    virtual void freeManifest(const char* data) = 0;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
