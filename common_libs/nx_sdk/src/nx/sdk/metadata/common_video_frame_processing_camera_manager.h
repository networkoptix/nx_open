#pragma once

#include <string>
#include <map>

#include <plugins/plugin_tools.h>
#include <nx/sdk/utils/debug.h>

#include "plugin.h"
#include "consuming_camera_manager.h"
#include "common_compressed_video_packet.h"
#include "common_uncompressed_video_frame.h"
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
    public nxpt::CommonRefCounter<ConsumingCameraManager>,
    protected nx::sdk::utils::Debug
{
protected:
    /**
     * @param enableOutput Enables NX_OUTPUT. Typically, use NX_DEBUG_ENABLE_OUTPUT as a value.
     * @param printPrefix Prefix for NX_PRINT and NX_OUTPUT. If empty, will be made from plugin's
     * libName().
     */
    CommonVideoFrameProcessingCameraManager(
        Plugin* plugin,
        bool enableOutput_,
        const std::string& printPrefix_ = "");

    virtual std::string capabilitiesManifest() = 0;

    /**
     * Override to accept next compressed video frame for processing. Should not block the caller
     * thread for long.
     * @param videoFrame Contains a pointer to the compressed video frame raw bytes. If the plugin
     *     manifest declares "needDeepCopyOfVideoFrames" in "capabilities", the lifetime (validity)
     *     of this pointer is the same as of videoFrame. Otherwise, the pointer is valid only until
     *     and during the subsequent call to pullMetadataPackets(), even if the lifetime of
     *     videoFrame is extended by addRef() or queryInterface() inside this method.
     */
    virtual bool pushCompressedVideoFrame(const CommonCompressedVideoPacket* /*videoFrame*/)
    {
        return true;
    }

    /**
     * Override to accept next uncompressed video frame for processing.
     * @param videoFrame Contains a pointer to the uncompressed video frame raw bytes. If the plugin
     *     manifest declares "needDeepCopyOfVideoFrames" in "capabilities", the lifetime (validity)
     *     of this pointer is the same as of videoFrame. Otherwise, the pointer is valid only until
     *     and during the subsequent call to pullMetadataPackets(), even if the lifetime of
     *     videoFrame is extended by addRef() or queryInterface() inside this method.
     */
    virtual bool pushUncompressedVideoFrame(const CommonUncompressedVideoFrame* /*videoFrame*/)
    {
        return true;
    }

    /**
     * Override to send the newly constructed metadata packets to Server - add the packets to the
     * provided list. Called after pushVideoFrame() to retrieve any metadata packets available to
     * the moment (not necessarily referring to that frame). As an alternative, send metadata to
     * Server by calling pushMetadataPacket() instead of implementing this method.
     */
    virtual bool pullMetadataPackets(std::vector<MetadataPacket*>* /*metadataPackets*/)
    {
        return true;
    }

    /**
     * Send a newly constructed metadata packet to Server. Can be called at any time, from any
     * thread. As an alternative, send metadata to Server by implementing pullMetadataPackets().
     */
    void pushMetadataPacket(MetadataPacket* metadataPacket);

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

public:
    virtual ~CommonVideoFrameProcessingCameraManager() override;

    /**
     * @return Parent plugin. The parent plugin is guaranteed to exist while any of its
     * CameraManagers exist, thus, this pointer is valid during the lifetime of this CameraManager.
     * Override to perform dynamic_cast to the actual descendant class of the plugin.
     */
    virtual Plugin* plugin() const { return m_plugin; }

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
    Plugin* const m_plugin;
    MetadataHandler* m_handler = nullptr;
    std::map<std::string, std::string> m_settings;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
