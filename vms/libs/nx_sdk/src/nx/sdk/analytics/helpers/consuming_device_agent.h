// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <nx/sdk/analytics/i_compound_metadata_packet.h>
#include <nx/sdk/analytics/i_compressed_video_packet.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/sdk/analytics/i_custom_metadata_packet.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_metadata_types.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/ptr.h>

namespace nx::sdk::analytics {

/**
 * Base class for a typical implementation of DeviceAgent which receives a stream and sends
 * back constructed metadata packets. Hides many technical details of Analytics SDK, but may
 * limit DeviceAgent capabilities - use only when suitable.
 *
 * To use NX_PRINT/NX_OUTPUT in a derived class with the prefix defined by this class, add the
 * following to the derived class .cpp:
 * <pre><code>
 *     #undef NX_PRINT_PREFIX
 *     #define NX_PRINT_PREFIX (this->logUtils.printPrefix)
 *     #include <nx/kit/debug.h>
 * </code></pre>
 */
class ConsumingDeviceAgent: public RefCountable<IConsumingDeviceAgent>
{
protected:
    const LogUtils logUtils;

protected:
    /**
     * @param enableOutput Enables NX_OUTPUT. Typically, use NX_DEBUG_ENABLE_OUTPUT as a value.
     * @param instanceId Must be non-empty only for Plugins from multi-IPlugin libraries.
     */
    ConsumingDeviceAgent(
        const IDeviceInfo* deviceInfo,
        bool enableOutput,
        const std::string& pluginInstanceId = "");

    virtual std::string manifestString() const = 0;

    /**
     * Override to accept next compressed video frame for processing. Should not block the caller
     * thread for long.
     * @param videoFrame Contains a pointer to the compressed video frame raw bytes.
     */
    virtual bool pushCompressedVideoFrame(const ICompressedVideoPacket* videoFrame);

    /**
     * Override to accept next uncompressed video frame for processing.
     * @param videoFrame Contains a pointer to the compressed video frame raw bytes.
     */
    virtual bool pushUncompressedVideoFrame(const IUncompressedVideoFrame* videoFrame);

    /**
     * Override to accept next custom metadata for processing.
     * @param customMetadataPacket Contains a pointer to the custom metadata packet.
     */
    virtual bool pushCustomMetadataPacket(const ICustomMetadataPacket* /*customMetadataPacket*/)
    {
        return true;
    }

    /**
     * Override to send the newly constructed metadata packets to Server - add the packets to the
     * provided list. Called after pushVideoFrame() to retrieve any metadata packets available to
     * the moment (not necessarily referring to that frame). As an alternative, send metadata to
     * Server by calling pushMetadataPacket() instead of implementing this method.
     */
    virtual bool pullMetadataPackets(std::vector<IMetadataPacket*>* metadataPackets);

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
        std::string description) const;

    /**
     * Called when the settings are received from the server (even if the values are not changed).
     * Should perform any required (re)initialization. Called even if the settings model is empty.
     * @return Error messages per setting (if any), as in IDeviceAgent::setSettings().
     */
    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived()
    {
        return nullptr;
    }

    /**
     * Provides access to the DeviceAgent settings stored by the Server for the particular Device.
     *
     * ATTENTION: If settingsReceived() has not been called yet, it means that the DeviceAgent has
     * not received its settings from the Server yet, and thus this method will yield empty values.
     *
     * @return Setting value, or an empty string if such setting does not exist, having logged the
     *     error.
     */
    std::string settingValue(const std::string& settingName) const;

    std::map<std::string, std::string> currentSettings() const;

    void pushManifest(const std::string& pushManifest);

    virtual void finalize() override;

    virtual void doGetSettingsOnActiveSettingChange(
        Result<const IActiveSettingChangedResponse*>* outResult,
        const IActiveSettingChangedAction* activeSettingChangedAction) override;

public:
    virtual ~ConsumingDeviceAgent() override;

//-------------------------------------------------------------------------------------------------
// Not intended to be used by the descendant.

public:
    virtual void setHandler(IHandler* handler) override;

protected:
    virtual void doPushDataPacket(Result<void>* outResult, IDataPacket* dataPacket) override;
    virtual void doSetSettings(
        Result<const ISettingsResponse*>* outResult, const IStringMap* settings) override;
    virtual void getPluginSideSettings(Result<const ISettingsResponse*>* outResult) const override;
    virtual void getManifest(Result<const IString*>* outResult) const override;

private:
    void logMetadataPacketIfNeeded(
        const IMetadataPacket* metadataPacket,
        int packetIndex) const;
    void processMetadataPackets(const std::vector<IMetadataPacket*>& metadataPackets);
    void processMetadataPacket(IMetadataPacket* metadataPacket, int packetIndex /*= -1*/);

private:
    mutable std::mutex m_mutex;
    Ptr<IDeviceAgent::IHandler> m_handler;
    std::map<std::string, std::string> m_settings;
};

} // namespace nx::sdk::analytics
