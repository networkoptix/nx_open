// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/helpers/ptr.h>

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
     * libName().
     */
    VideoFrameProcessingDeviceAgent(
        IEngine* engine,
        const IDeviceInfo* deviceInfo,
        bool enableOutput,
        const std::string& printPrefix = "");

    virtual std::string manifestInternal() const = 0;

    /**
     * Override to accept next compressed video frame for processing. Should not block the caller
     * thread for long.
     * @param videoFrame Contains a pointer to the compressed video frame raw bytes. The lifetime
     *     (validity) of this pointer is the same as of videoFrame. Thus, it can be extended by
     *     addRef() or queryInterface() inside this method.
     */
    virtual bool pushCompressedVideoFrame(const ICompressedVideoPacket* /*videoFrame*/)
    {
        return true;
    }

    /**
     * Override to accept next uncompressed video frame for processing.
     * @param videoFrame Contains a pointer to the compressed video frame raw bytes. The lifetime
     *     (validity) of this pointer is the same as of videoFrame. Thus, it can be extended by
     *     addRef() or queryInterface() inside this method.
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
     */
    virtual Result<const IStringMap*> settingsReceived() { return nullptr; }

    /**
     * Provides access to the Manager settings stored by the server for a particular Resource.
     * ATTENTION: If settingsReceived() has not been called yet, it means that the DeviceAgent has
     * not received its settings from the server yet, and thus this method will yield empty values.
     * @return Param value, or an empty string if such param does not exist, having logged the
     *     error.
     */
    std::string getParamValue(const std::string& paramName);

public:
    virtual ~VideoFrameProcessingDeviceAgent() override;

    /**
     * Intended to be called from a method of a derived class overriding engine().
     * @return Parent Engine, casted to the specified type.
     */
    template<typename DerivedEngine>
    DerivedEngine* engineCasted() const
    {
        const auto engine = dynamic_cast<DerivedEngine*>(m_engine);
        assertEngineCasted(engine);
        return engine;
    }

    virtual IEngine* engine() const override { return m_engine; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by the descendant.

public:
    virtual void setHandler(IDeviceAgent::IHandler* handler) override;
    virtual Result<void> pushDataPacket(IDataPacket* dataPacket) override;
    virtual Result<const IString*> manifest() const override;
    virtual Result<const IStringMap*> setSettings(const IStringMap* settings) override;
    virtual Result<const ISettingsResponse*> pluginSideSettings() const override;

private:
    void assertEngineCasted(void* engine) const;
    void logMetadataPacketIfNeeded(
        const IMetadataPacket* metadataPacket,
        const std::string& packetIndexName) const;
    void processMetadataPackets(const std::vector<IMetadataPacket*>& metadataPackets);
    void processMetadataPacket(IMetadataPacket* metadataPacket, int packetIndex /*= -1*/);

private:
    mutable std::mutex m_mutex;
    IEngine* const m_engine;
    nx::sdk::Ptr<IDeviceAgent::IHandler> m_handler;
    std::map<std::string, std::string> m_settings;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
