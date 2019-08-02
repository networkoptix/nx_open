// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/ptr.h>

#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/sdk/analytics/i_compound_metadata_packet.h>
#include <nx/sdk/analytics/i_metadata_types.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Base class for a typical implementation of DeviceAgent which receives video frames and sends
 * back constructed metadata packets. Hides many technical details of Analytics SDK, but may
 * limit DeviceAgent capabilities - use only when suitable.
 *
 * To use NX_PRINT/NX_OUTPUT in a derived class with the prefix defined by this class, add the
 * following to the derived class .cpp:
 * <pre><code>
 *     #define NX_PRINT_PREFIX (this->logUtils.printPrefix)
 *     #include <nx/kit/debug.h>
 * </code></pre>
 */
class VideoFrameProcessingDeviceAgent: public RefCountable<IConsumingDeviceAgent>
{
protected:
    const LogUtils logUtils;

protected:
    /**
     * @param enableOutput Enables NX_OUTPUT. Typically, use NX_DEBUG_ENABLE_OUTPUT as a value.
     * @param printPrefix Prefix for NX_PRINT and NX_OUTPUT. If empty, will be made from Engine's
     *     libName().
     */
    VideoFrameProcessingDeviceAgent(
        const IDeviceInfo* deviceInfo,
        bool enableOutput,
        const std::string& printPrefix = "");

    virtual std::string manifestString() const = 0;

    /**
     * Override to accept next compressed video frame for processing. Should not block the caller
     * thread for long.
     * @param videoFrame Contains a pointer to the compressed video frame raw bytes.
     */
    virtual bool pushCompressedVideoFrame(const ICompressedVideoPacket* /*videoFrame*/)
    {
        return true;
    }

    /**
     * Override to accept next uncompressed video frame for processing.
     * @param videoFrame Contains a pointer to the compressed video frame raw bytes.
     */
    virtual bool pushUncompressedVideoFrame(const IUncompressedVideoFrame* /*videoFrame*/)
    {
        return true;
    }

    /**
     * Override to send the newly constructed metadata packets to Server - add the packets to the
     * provided list. Called after pushVideoFrame() to retrieve any metadata packets available to
     * the moment (not necessarily referring to that frame). As an alternative, send metadata to
     * Server by calling pushMetadataPacket() instead of implementing this method.
     */
    virtual bool pullMetadataPackets(std::vector<IMetadataPacket*>* /*metadataPackets*/)
    {
        return true;
    }

    /**
     * Send a newly constructed metadata packet to Server. Can be called at any time, from any
     * thread. As an alternative, send metadata to Server by implementing pullMetadataPackets().
     */
    void pushMetadataPacket(IMetadataPacket* metadataPacket);

    /**
     * Sends a PluginDiagnosticEvent to the Server. Can be called from any thread, but if called
     * before settingsReceived() was called, will be ignored in case setHandler() was not called
     * yet.
     */
    void pushPluginDiagnosticEvent(
        IPluginDiagnosticEvent::Level level,
        std::string caption,
        std::string description);

    /**
     * Called when the settings are received from the server (even if the values are not changed).
     * Should perform any required (re)initialization. Called even if the settings model is empty.
     * @return Error messages per setting (if any), as in IDeviceAgent::setSettings().
     */
    virtual nx::sdk::Result<const nx::sdk::IStringMap*> settingsReceived() { return nullptr; }

    /**
     * Provides access to the DeviceAgent settings stored by the Server for the particular Device.
     *
     * ATTENTION: If settingsReceived() has not been called yet, it means that the DeviceAgent has
     * not received its settings from the Server yet, and thus this method will yield empty values.
     *
     * @return Setting value, or an empty string if such setting does not exist, having logged the
     *     error.
     */
    std::string settingValue(const std::string& paramName);

public:
    virtual ~VideoFrameProcessingDeviceAgent() override;

//-------------------------------------------------------------------------------------------------
// Not intended to be used by the descendant.

public:
    virtual void setHandler(IHandler* handler) override;
    
protected:
    virtual void doPushDataPacket(Result<void>* outResult, IDataPacket* dataPacket) override;
    virtual void doSetSettings(
        Result<const IStringMap*>* outResult, const IStringMap* settings) override;
    virtual void getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const override;
    virtual void getManifest(Result<const IString*>* outResult) const override;

private:
    void logMetadataPacketIfNeeded(
        const IMetadataPacket* metadataPacket,
        const std::string& packetIndexName) const;
    void processMetadataPackets(const std::vector<IMetadataPacket*>& metadataPackets);
    void processMetadataPacket(IMetadataPacket* metadataPacket, int packetIndex /*= -1*/);

private:
    mutable std::mutex m_mutex;
    Ptr<IDeviceAgent::IHandler> m_handler;
    std::map<std::string, std::string> m_settings;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
