#pragma once

#include <plugins/plugin_api.h>
#include <nx/sdk/common.h>

#include "camera_manager.h"

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements Plugin interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_Plugin =
    {{0x6d,0x73,0x71,0x36,0x17,0xad,0x43,0xf9,0x9f,0x80,0x7d,0x56,0x91,0x36,0x82,0x94}};

/**
 * Main interface for a Metadata Plugin instance. The instance is created by Mediaserver on its
 * start via a call to createNxMetadataPlugin() which should be exported as extern "C" by the
 * plugin library, and is destroyed (via releaseRef()) on Mediaserver shutdown. Mediaserver never
 * creates more than one instance of Plugin.
 */
class Plugin: public nxpl::Plugin3
{
public:
    /**
     * Creates, or returns already existing, a CameraManager for the given camera. There must be
     * only one instance of the CameraManager per camera at any given time. It means that if we
     * pass CameraInfo objects with the same UID multiple times, then the pointer to exactly the
     * same object must be returned. Also, multiple cameras must not share the same CameraManager.
     * It means that if we pass CameraInfo objects with different UIDs, then pointers to different
     * objects must be returned.
     * @param cameraInfo Information about the camera for which a CameraManager should be created.
     * @param outError Status of the operation; set to noError before this call.
     * @return Pointer to an object that implements CameraManager interface, or null in case of
     * failure.
     */
    virtual CameraManager* obtainCameraManager(const CameraInfo& cameraInfo, Error* outError) = 0;

    /**
     * @brief provides null terminated UTF-8 string containing json manifest
     * according to nx_metadata_plugin_manifest.schema.json.
     * @return pointer to c-style string which MUST be valid till plugin object exists
     */
    virtual const char* capabilitiesManifest(Error* error) const = 0;

    /**
     * Called before other methods. Server provides the set of settings stored in its database for
     * the plugin type.
     * @param settings Values of settings declared in the manifest. The pointer is valid only
     *     during the call.
     */
    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) = 0;
};

typedef nxpl::PluginInterface* (*CreateNxMetadataPluginProc)();

} // namespace metadata
} // namespace sdk
} // namespace nx
