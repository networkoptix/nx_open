#include "video_frame_processing_device_agent.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->logUtils.enableOutput)
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/sdk/analytics/helpers/engine.h>
#include <nx/sdk/helpers/plugin_event.h>
#include <nx/sdk/analytics/i_plugin.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>

namespace nx {
namespace sdk {
namespace analytics {

class PrintPrefixMaker
{
public:
    std::string makePrintPrefix(
        const std::string& overridingPrintPrefix,
        const IEngine* engine,
        const IDeviceInfo* deviceInfo)
    {
        NX_KIT_ASSERT(engine);
        NX_KIT_ASSERT(engine->plugin());
        NX_KIT_ASSERT(engine->plugin()->name());
        NX_KIT_ASSERT(deviceInfo);

        if (!overridingPrintPrefix.empty())
            return overridingPrintPrefix;

        return std::string("[") + engine->plugin()->name()
            + "_device_" + deviceInfo->id() + "] ";
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
    const IDeviceInfo* deviceInfo,
    bool enableOutput,
    const std::string& printPrefix)
    :
    logUtils(enableOutput, PrintPrefixMaker().makePrintPrefix(printPrefix, engine, deviceInfo)),
    m_engine(engine)
{
    NX_PRINT << "Created " << this;
}

VideoFrameProcessingDeviceAgent::~VideoFrameProcessingDeviceAgent()
{
    NX_PRINT << "Destroyed " << this;
}

//-------------------------------------------------------------------------------------------------
// Implementation of interface methods.

Error VideoFrameProcessingDeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handler = handler;
    return Error::noError;
}

Error VideoFrameProcessingDeviceAgent::pushDataPacket(IDataPacket* dataPacket)
{
    const auto logError =
        [this, func = __func__](Error error, const std::string& message = "")
        {
            if (!message.empty() || error != Error::noError || (NX_DEBUG_ENABLE_OUTPUT))
            {
                NX_PRINT << func << "() " << ((NX_DEBUG_ENABLE_OUTPUT) ? "END " : "") << "-> "
                    << error << (message.empty() ? "" : ": ") << message;
            }
            return error;
        };

    NX_OUTPUT << __func__ << "() BEGIN";

    if (!dataPacket)
        return logError(Error::unknownError, "dataPacket is null; discarding it.");

    if (dataPacket->timestampUs() < 0)
    {
        return logError(Error::unknownError, "dataPacket has invalid timestamp "
            + nx::kit::utils::toString(dataPacket->timestampUs()) + "; discarding the packet.");
    }

    if (const auto compressedFrame = queryInterfacePtr<ICompressedVideoPacket>(dataPacket))
    {
        if (!pushCompressedVideoFrame(compressedFrame.get()))
            return logError(Error::unknownError, "pushCompressedVideoFrame() failed.");
    }
    else if (const auto uncompressedFrame = queryInterfacePtr<IUncompressedVideoFrame>(dataPacket))
    {
        if (!pushUncompressedVideoFrame(uncompressedFrame.get()))
            return logError(Error::unknownError, "pushUncompressedVideoFrame() failed.");
    }
    else
    {
        return logError(Error::noError, "Server ERROR: Unsupported frame supplied; ignored.");
    }

    if (!m_handler)
        return logError(Error::unknownError, "Server ERROR: setMetadataHandler() was not called.");

    std::vector<IMetadataPacket*> metadataPackets;
    if (!pullMetadataPackets(&metadataPackets))
        return logError(Error::unknownError, "pullMetadataPackets() failed.");

    processMetadataPackets(metadataPackets);

    return logError(Error::noError);
}

void VideoFrameProcessingDeviceAgent::processMetadataPackets(
    const std::vector<IMetadataPacket*>& metadataPackets)
{
    if (!metadataPackets.empty())
    {
        NX_OUTPUT << __func__ << "(): Producing " << metadataPackets.size()
            << " metadata packet(s).";
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = 0; i < (int) metadataPackets.size(); ++i)
            processMetadataPacket(toPtr(metadataPackets.at(i)).get(), i);
    }
}

void VideoFrameProcessingDeviceAgent::processMetadataPacket(
    IMetadataPacket* metadataPacket, int packetIndex = -1)
{
    const std::string packetIndexName =
        (packetIndex == -1) ? "" : (std::string(" #") + nx::kit::utils::toString(packetIndex));
    if (!m_handler)
    {
        NX_PRINT << __func__ << "(): "
            << "INTERNAL ERROR: setHandler() was not called; ignoring the packet";
        return;
    }
    if (!metadataPacket)
    {
        NX_OUTPUT << __func__ << "(): WARNING: Null metadata packet" << packetIndexName
            << " found; discarded.";
        return;
    }

    if (NX_DEBUG_ENABLE_OUTPUT)
    {
        std::string packetName;
        if (queryInterfacePtr<IObjectMetadataPacket>(metadataPacket))
        {
            packetName = "Object";
        }
        else if (queryInterfacePtr<IEventMetadataPacket>(metadataPacket))
        {
            packetName = "Event";
        }
        else
        {
            NX_OUTPUT << __func__ << "(): WARNING: Metadata packet" << packetIndexName
                << " has unknown type.";
            packetName = "Unknown";
        }
        packetName += " metadata packet" + packetIndexName;

        auto compoundMetadataPacket = queryInterfacePtr<ICompoundMetadataPacket>(metadataPacket);
        if (compoundMetadataPacket)
        {

            if (compoundMetadataPacket->count() == 0)
            {
                NX_OUTPUT << __func__ << "(): WARNING: " << packetName << " is empty.";
                return;
            }

            const std::string itemsName = (compoundMetadataPacket->count() == 1)
                ? (std::string("item of type ") + compoundMetadataPacket->at(0)->typeId())
                : "item(s)";

            NX_OUTPUT << __func__ << "(): " << packetName << " contains "
                << compoundMetadataPacket->count() << " " << itemsName << ".";

            if (metadataPacket->timestampUs() == 0)
                NX_OUTPUT << __func__ << "(): WARNING: " << packetName << " has timestamp 0.";
        }
    }

    NX_KIT_ASSERT(metadataPacket->timestampUs() >= 0);

    m_handler->handleMetadata(metadataPacket);
}

const IString* VideoFrameProcessingDeviceAgent::manifest(Error* /*error*/) const
{
    return new String(manifest());
}

void VideoFrameProcessingDeviceAgent::setSettings(const IStringMap* settings)
{
    if (!logUtils.convertAndOutputStringMap(&m_settings, settings, "Received settings"))
        return; //< The error is already logged.

    settingsReceived();
}

IStringMap* VideoFrameProcessingDeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

//-------------------------------------------------------------------------------------------------
// Tools for the derived class.

void VideoFrameProcessingDeviceAgent::pushMetadataPacket(
    IMetadataPacket* metadataPacket)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    processMetadataPacket(metadataPacket);
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
    // This method is placed in .cpp to allow NX_KIT_ASSERT() use the correct NX_PRINT() prefix.
    NX_KIT_ASSERT(engine,
        "nx::sdk::analytics::VideoFrameProcessingDeviceAgent "
        + nx::kit::utils::toString(this)
        + " has m_engine of incorrect runtime type " + typeid(*m_engine).name());
}

} // namespace analytics
} // namespace sdk
} // namespace nx
