#pragma once

#include <string>
#include <map>

#include <plugins/plugin_tools.h>

#include "plugin.h"
#include "consuming_camera_manager.h"
#include "common_compressed_video_packet.h"
#include "objects_metadata_packet.h"

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Base class for a typical implementation of CameraManager which receives video frames and sends
 * back constructed metadata packets. Hides many technical details of Metadata Plugin SDK, but may
 * limit CameraManager capabilities - use only when suitable.
 */
class CommonVideoFrameProcessingCameraManager:
    public nxpt::CommonRefCounter<ConsumingCameraManager>
{
protected:
    CommonVideoFrameProcessingCameraManager(Plugin* plugin): m_plugin(plugin) {}

    virtual std::string capabilitiesManifest() = 0;

    /**
     * Override to accept next video frame for processing. The provided pointer is valid only until
     * a subsequent call to pullMetadataPackets(), as well as the actual byte data pointer inside,
     * regardless of the frame-deep-copy policy defined by the manifest.
     */
    virtual bool pushVideoFrame(const CommonCompressedVideoPacket* videoFrame) { return true; }

    /**
     * Override to send the newly constructed metadata packets to Server - add the packets to the
     * provided list. Called after pushVideoFrame() to retrieve any metadata packets available to
     * the moment (not necessarily referring to that frame). As an alternative, send metadata to
     * Server by calling pushMetadataPacket() instead of implementing this method.
     */
    virtual bool pullMetadataPackets(std::vector<MetadataPacket*>* metadataPackets)
    {
        return true;
    }

    /**
     * Send a newly constructed metadata packet to Server. Can be called at any time, from any
     * thread. As an alternative, send metadata to Server by implementing pullMetadataPackets().
     */
    void pushMetadataPacket(sdk::metadata::MetadataPacket* metadataPacket);

    /**
     * Called when any of the seetings (param values) change.
     */
    virtual void settingsChanged() {}

    /**
     * Provides access to the Manager settings stored by the server for a particular Resource.
     * @return Param value, or an empty string if such param does not exist, having logged the
     * error.
     */
    std::string getParamValue(const char* paramName);

    /** Enable or disable verbose debug output via NX_OUTPUT from methods of this class. */
    void setEnableOutput(bool value) { m_enableOutput = value; }

    Plugin* plugin() const { return m_plugin; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by the descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual Error setHandler(MetadataHandler* handler) override;
    virtual Error pushDataPacket(DataPacket* dataPacket) override;
    virtual Error startFetchingMetadata(nxpl::NX_GUID* typeList, int typeListSize) override;
    virtual Error stopFetchingMetadata() override;
    virtual const char* capabilitiesManifest(Error* error) override;
    virtual void freeManifest(const char* data) override;
    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;

private:
    bool m_enableOutput = false;
    Plugin* m_plugin;
    MetadataHandler* m_handler = nullptr;
    std::map<std::string, std::string> m_settings;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
