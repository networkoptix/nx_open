#pragma once

#include <plugins/plugin_api.h>
#include <nx/sdk/common.h>

#include "camera_manager.h"

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Each class that implements Action interface should properly handle this GUID in its
 * queryInterface().
 */
static const nxpl::NX_GUID IID_Action =
    {{0x92,0xF4,0x7D,0x22,0x1A,0x57,0x43,0xC6,0xB8,0x43,0xF4,0x27,0xB2,0x1B,0xD0,0x3F}};

/**
 * Interface to an object supplied to Plugin::executeAction().
 */
class Action: public nxpl::PluginInterface
{
public:
    /** Id of the action being triggered. */
    virtual const char* actionId() = 0;

    /** Id of a metadata object for which the action has been triggered. */
    virtual nxpl::NX_GUID objectId() = 0;

    /** Id of a camera from which the action has been triggered. */
    virtual nxpl::NX_GUID cameraId() = 0;

    /** Timestamp of a video frame from which the action has been triggered. */
    virtual int64_t timestampUs() = 0;

    /**
     * If the plugin manifest defines params for this action type, contains the array of their
     * values after they are filled by the user via Client form. Otherwise, null.
     */
    virtual const nxpl::Setting* params() = 0;

    /** Number of items in params() array. */
    virtual int paramCount() = 0;

    /**
     * Report action result back to Server. If the action is decided not to have any result, this
     * method can be either called with nulls or not called at all.
     * @param actionUrl If not null, Client will open this URL in an embedded browser.
     * @param messageToUser If not null, Client will show this text to the user.
     */
    virtual void handleResult(
        const char* actionUrl,
        const char* messageToUser) = 0;
};

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
     * @param outError Status of the operation; should be set to noError before this call.
     * @return Pointer to an object that implements CameraManager interface, or null in case of
     * failure.
     */
    virtual CameraManager* obtainCameraManager(const CameraInfo* cameraInfo, Error* outError) = 0;

    /**
     * Provides null terminated UTF-8 string containing json manifest according to
     * nx_metadata_plugin_manifest.schema.json.
     * @param outError Status of the operation; should be set to noError before this call.
     * @return Pointer to c-style string which MUST be valid while Plugin object exists.
     */
    virtual const char* capabilitiesManifest(Error* outError) const = 0;

    /**
     * Called before other methods. Server provides the set of settings stored in its database for
     * the plugin type.
     * @param settings Values of settings declared in the manifest. The pointer is valid only
     *     during the call. If count is 0, the pointer is null.
     */
    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) = 0;

    /**
     * Action handler. Called when some action defined by this plugin is triggered by Server.
     * @param action Provides data for the action such as metadata object for which the action has
     *     been triggered, and a means for reporting back action results to Server. This object
     *     should not be used after returning from this function.
     * @param outError Status of the operation; should be set to noError before this call.
     */
    virtual void executeAction(Action* action, Error* outError) = 0;
};

typedef nxpl::PluginInterface* (*CreateNxMetadataPluginProc)();

} // namespace metadata
} // namespace sdk
} // namespace nx
