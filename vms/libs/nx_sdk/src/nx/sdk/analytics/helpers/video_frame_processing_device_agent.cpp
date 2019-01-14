#include "video_frame_processing_device_agent.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->logUtils.enableOutput)
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/helpers/plugin_event.h>
#include <nx/sdk/analytics/i_plugin.h>

namespace nx {
namespace sdk {
namespace analytics {

class PrintPrefixMaker
{
public:
    std::string makePrintPrefix(const std::string& overridingPrintPrefix, const IEngine* engine)
    {
        NX_KIT_ASSERT(engine);
        NX_KIT_ASSERT(engine->plugin());
        NX_KIT_ASSERT(engine->plugin()->name());

        if (!overridingPrintPrefix.empty())
            return overridingPrintPrefix;
        return std::string("[") + engine->plugin()->name() + " DeviceAgent] ";
    }

private:
    /** Used by the above NX_KIT_ASSERT (via NX_PRINT). */
    struct
    {
        const std::string printPrefix =
            "nx::sdk::analytics::VideoFrameProcessingDeviceAgent(): ";
    } logUtils;
};

VideoFrameProcessingDeviceAgent::VideoFrameProcessingDeviceAgent(
    IEngine* engine,
    bool enableOutput,
    const std::string& printPrefix)
    :
    logUtils(enableOutput, PrintPrefixMaker().makePrintPrefix(printPrefix, engine)),
    m_engine(engine)
{
    NX_PRINT << "Created " << this;
}

VideoFrameProcessingDeviceAgent::~VideoFrameProcessingDeviceAgent()
{
    NX_PRINT << "Destroyed " << this;
}

//-------------------------------------------------------------------------------------------------
// Implementations of interface methods.

void* VideoFrameProcessingDeviceAgent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_DeviceAgent)
    {
        addRef();
        return static_cast<IDeviceAgent*>(this);
    }

    if (interfaceId == IID_ConsumingDeviceAgent)
    {
        addRef();
        return static_cast<IConsumingDeviceAgent*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

Error VideoFrameProcessingDeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
    return Error::noError;
}

Error VideoFrameProcessingDeviceAgent::pushDataPacket(IDataPacket* dataPacket)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    if (!dataPacket)
    {
        NX_PRINT << __func__ << "() END -> unknownError: INTERNAL ERROR: dataPacket is null; "
            << "discarding the packet.";
        return Error::unknownError;
    }

    if (dataPacket->timestampUs() < 0)
    {
        NX_PRINT << __func__ << "() END -> unknownError: INTERNAL ERROR: "
            << "dataPacket has invalid timestamp " << dataPacket->timestampUs()
            << "; discarding the packet.";
        return Error::unknownError;
    }

    if (const auto compressedFrame = Ptr<ICompressedVideoPacket>(
        dataPacket->queryInterface(IID_CompressedVideoPacket)))
    {
        if (!pushCompressedVideoFrame(compressedFrame.get()))
        {
            NX_OUTPUT << __func__ << "() END -> unknownError: pushCompressedVideoFrame() failed.";
            return Error::unknownError;
        }
    }
    else if (const auto uncompressedFrame = Ptr<IUncompressedVideoFrame>(
        dataPacket->queryInterface(IID_UncompressedVideoFrame)))
    {
        if (!pushUncompressedVideoFrame(uncompressedFrame.get()))
        {
            NX_PRINT << __func__ << "() END -> unknownError: pushUncompressedVideoFrame() failed.";
            return Error::unknownError;
        }
    }
    else
    {
        NX_OUTPUT << __func__ << "() END -> noError: Unsupported frame supplied; ignored.";
        return Error::noError;
    }

    std::vector<IMetadataPacket*> metadataPackets;
    if (!pullMetadataPackets(&metadataPackets))
    {
        NX_PRINT << __func__ << "() END -> unknownError: pullMetadataPackets() failed.";
        return Error::unknownError;
    }

    if (!metadataPackets.empty())
    {
        NX_OUTPUT << __func__ << "() Producing " << metadataPackets.size()
            << " metadata packet(s).";
    }

    if (!m_handler)
    {
        NX_PRINT << __func__ << "() END -> unknownError: setMetadataHandler() was not called.";
        return Error::unknownError;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& metadataPacket: metadataPackets)
        {
            if (metadataPacket)
            {
                NX_KIT_ASSERT(metadataPacket->timestampUs() >= 0);
                if (metadataPacket->timestampUs() == 0)
                    NX_OUTPUT << __func__ << "(): WARNING: Metadata packet has timestamp 0.";
                m_handler->handleMetadata(metadataPacket);
                metadataPacket->releaseRef();
            }
            else
            {
                NX_OUTPUT << __func__ << "(): WARNING: Null metadata packet found; discarded.";
            }
        }
    }

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

const IString* VideoFrameProcessingDeviceAgent::manifest(Error* /*error*/) const
{
    return new String(manifest());
}

void VideoFrameProcessingDeviceAgent::setSettings(const nx::sdk::IStringMap* settings)
{
    if (!logUtils.convertAndOutputStringMap(&m_settings, settings, "Received settings"))
        return; //< The error is already logged.

    settingsReceived();
}

nx::sdk::IStringMap* VideoFrameProcessingDeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

//-------------------------------------------------------------------------------------------------
// Tools for the derived class.

void VideoFrameProcessingDeviceAgent::pushMetadataPacket(
    nx::sdk::analytics::IMetadataPacket* metadataPacket)
{
    if (!metadataPacket)
    {
        NX_PRINT << __func__ << "(): ERROR: metadataPacket is null; ignoring";
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handler)
    {
        NX_PRINT << __func__ << "(): "
            << "INTERNAL ERROR: setHandler() was not called; ignoring the packet";
    }
    else
    {
        m_handler->handleMetadata(metadataPacket);
    }

    metadataPacket->releaseRef();
}

void VideoFrameProcessingDeviceAgent::pushPluginEvent(
    IPluginEvent::Level level,
    std::string caption,
    std::string description)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_handler)
    {
        NX_PRINT << __func__ << "(): "
            << "INTERNAL ERROR: setHandler() was not called; ignoring plugin event";
        return;
    }

    const auto event = makePtr<PluginEvent>(
        level, caption, description);
    m_handler->handlePluginEvent(event.get());
}

// TODO: Consider making a template with param type, checked according to the manifest.
std::string VideoFrameProcessingDeviceAgent::getParamValue(const char* paramName)
{
    return m_settings[paramName];
}

void VideoFrameProcessingDeviceAgent::assertEngineCasted(void* engine) const
{
    NX_KIT_ASSERT(engine,
        "nx::sdk::analytics::VideoFrameProcessingDeviceAgent "
        + nx::kit::utils::toString(this)
        + " has m_engine of incorrect runtime type " + typeid(*m_engine).name());
}

} // namespace analytics
} // namespace sdk
} // namespace nx
