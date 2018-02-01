#pragma once

#include <plugins/plugin_api.h>
#include <nx/sdk/common.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements Plugin interface
 * should properly handle this GUID in its queryInterface method
 */
static const nxpl::NX_GUID IID_Plugin = {{0x6d, 0x73, 0x71, 0x36, 0x17, 0xad, 0x43, 0xf9,
    0x9f, 0x80, 0x7d, 0x56, 0x91, 0x36, 0x82, 0x94}};

/**
 * @brief The Plugin class is a main interface for metadata plugins.
 * Each metadata plugin should implement this interface.
 */
class Plugin: public nxpl::Plugin3
{
public:
    /**
     * @brief obtainCameraManager creates (or return already existing)
     * metadata manager for the given resource.
     * There MUST be only one manager per resource at the same time.
     * It means that if we pass resource infos with the same UID multiple times
     * then pointer to exactly the same object MUST be returned.
     * Multiple resources MUST NOT share the same manager.
     * It means that if we pass resource infos with different UIDs
     * then pointers to different objects MUST be returned.
     * @param resourceInfo information about resource for which metadata manager should be created.
     * @param error status of operation.
     * noError in case of success and some other value in case of failure.
     * @return pointer to object that implements CameraManager interface
     * or nullptr in case of failure.
     */
    virtual CameraManager* obtainCameraManager(
        const CameraInfo& cameraInfo,
        Error* outError) = 0;

    /**
     * @brief provides null terminated UTF8 string containing json manifest
     * according to nx_metadata_plugin_manifest.schema.json.
     * @return pointer to c-style string which MUST be valid till plugin object exists
     */
    virtual const char* capabilitiesManifest(Error* error) const = 0;
};

typedef nxpl::PluginInterface* (*CreateNxMetadataPluginProc)();

} // namespace metadata
} // namespace sdk
} // namespace nx
