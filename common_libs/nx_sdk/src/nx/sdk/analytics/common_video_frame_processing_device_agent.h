#pragma once

#include <string>
#include <map>
#include <vector>

#include <plugins/plugin_tools.h>
#include <nx/sdk/utils.h>

#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/consuming_device_agent.h>
#include <nx/sdk/analytics/objects_metadata_packet.h>
#include <nx/sdk/analytics/compressed_video_packet.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Base class for a typical implementation of DeviceAgent which receives video frames and sends
 * back constructed metadata packets. Hides many technical details of Metadata Plugin SDK, but may
 * limit DeviceAgent capabilities - use only when suitable.
 *
 * To use NX_PRINT/NX_OUTPUT in a derived class with the same printPrefix as used in this class,
 * add the following to the derived class cpp:
 * <pre><code>
 *     #define NX_PRINT_PREFIX (this->utils.printPrefix)
 *     #include <nx/kit/debug.h>
 * </code></pre>
 */
class CommonVideoFrameProcessingDeviceAgent:
    public nxpt::CommonRefCounter<ConsumingDeviceAgent>
{
protected:
    const nx::sdk::Utils utils;

protected:
    /**
     * @param enableOutput Enables NX_OUTPUT. Typically, use NX_DEBUG_ENABLE_OUTPUT as a value.
     * @param printPrefix Prefix for NX_PRINT and NX_OUTPUT. If empty, will be made from Engine's
     * libName().
     */
    CommonVideoFrameProcessingDeviceAgent(
        Engine* engine,
        bool enableOutput,
        const std::string& printPrefix = "");

    virtual std::string manifest() = 0;

    /**
     * Override to accept next compressed video frame for processing. Should not block the caller
     * thread for long.
     * @param videoFrame Contains a pointer to the compressed video frame raw bytes. The lifetime
     *     (validity) of this pointer is the same as of videoFrame. Thus, it can be extended by
     *     addRef() or queryInterface() inside this method.
     */
    virtual bool pushCompressedVideoFrame(const CompressedVideoPacket* /*videoFrame*/)
    {
        return true;
    }

    /**
     * Override to accept next uncompressed video frame for processing.
     * @param videoFrame Contains a pointer to the compressed video frame raw bytes. The lifetime
     *     (validity) of this pointer is the same as of videoFrame. Thus, it can be extended by
     *     addRef() or queryInterface() inside this method.
     */
    virtual bool pushUncompressedVideoFrame(const UncompressedVideoFrame* /*videoFrame*/)
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
     * Called when any of the settings (param values) change.
     */
    virtual void settingsChanged() {}

    /**
     * Provides access to the Manager settings stored by the server for a particular Resource.
     * ATTENTION: If settingsChanged() has not been called yet, it means that the DeviceAgent has
     * not received its settings from the server yet, and thus this method will yield empty values.
     * @return Param value, or an empty string if such param does not exist, having logged the
     *     error.
     */
    std::string getParamValue(const char* paramName);

public:
    virtual ~CommonVideoFrameProcessingDeviceAgent() override;

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

    virtual Engine* engine() const override { return m_engine; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by the descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual Error setMetadataHandler(MetadataHandler* handler) override;
    virtual Error pushDataPacket(DataPacket* dataPacket) override;
    virtual Error startFetchingMetadata(const char* const* typeList, int typeListSize) override;
    virtual Error stopFetchingMetadata() override;
    virtual const char* manifest(Error* error) override;
    virtual void freeManifest(const char* data) override;
    virtual void setSettings(const nx::sdk::Settings* settings) override;
    virtual nx::sdk::Settings* settings() const override;

private:
    void assertEngineCasted(void* engine) const;

private:
    Engine* const m_engine;
    MetadataHandler* m_handler = nullptr;
    std::map<std::string, std::string> m_settings;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
