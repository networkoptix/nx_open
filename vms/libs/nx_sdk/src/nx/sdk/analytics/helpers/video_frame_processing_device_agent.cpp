// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_frame_processing_device_agent.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->logUtils.enableOutput)
#include <nx/kit/debug.h>

#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/to_string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/sdk/analytics/helpers/engine.h>

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
        const IDeviceInfo* deviceInfo)
    {
        NX_KIT_ASSERT(deviceInfo);

        if (!overridingPrintPrefix.empty())
            return overridingPrintPrefix;

        return std::string("[") + libContext().name() + "_device_" + deviceInfo->id() + "] ";
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
    logUtils(enableOutput, PrintPrefixMaker().makePrintPrefix(printPrefix, deviceInfo)),
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

void VideoFrameProcessingDeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    handler->addRef();
    m_handler.reset(handler);
}

Result<void> VideoFrameProcessingDeviceAgent::pushDataPacket(IDataPacket* dataPacket)
{
    const auto logError =
        [this, func = __func__](ErrorCode errorCode, const std::string& message)
        {
            NX_PRINT << func << "() " << ((NX_DEBUG_ENABLE_OUTPUT) ? "END " : "") << "-> "
                << errorCode << ": " << message;
            return error(errorCode, message);
        };

    NX_OUTPUT << __func__ << "() BEGIN";

    if (!dataPacket)
        return logError(ErrorCode::invalidParams, "dataPacket is null; discarding it.");

    if (dataPacket->timestampUs() < 0)
    {
        return logError(ErrorCode::invalidParams, "dataPacket has invalid timestamp "
            + nx::kit::utils::toString(dataPacket->timestampUs()) + "; discarding the packet.");
    }

    if (const auto compressedFrame = queryInterfacePtr<ICompressedVideoPacket>(dataPacket))
    {
        if (!pushCompressedVideoFrame(compressedFrame.get()))
            return logError(ErrorCode::otherError, "pushCompressedVideoFrame() failed.");
    }
    else if (const auto uncompressedFrame = queryInterfacePtr<IUncompressedVideoFrame>(dataPacket))
    {
        if (!pushUncompressedVideoFrame(uncompressedFrame.get()))
            return logError(ErrorCode::otherError, "pushUncompressedVideoFrame() failed.");
    }
    else
    {
        return logError(ErrorCode::invalidParams, "Unsupported frame supplied; ignored.");
    }

    if (!m_handler)
        return logError(ErrorCode::internalError, "setHandler() was not called.");

    std::vector<IMetadataPacket*> metadataPackets;
    if (!pullMetadataPackets(&metadataPackets))
        return logError(ErrorCode::otherError, "pullMetadataPackets() failed.");

    processMetadataPackets(metadataPackets);

    NX_OUTPUT << __func__ << "() END";
    return {};
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

    logMetadataPacketIfNeeded(metadataPacket, packetIndexName);
    NX_KIT_ASSERT(metadataPacket->timestampUs() >= 0);
    m_handler->handleMetadata(metadataPacket);
}

StringResult VideoFrameProcessingDeviceAgent::manifest() const
{
    return new String(manifestString());
}

StringMapResult VideoFrameProcessingDeviceAgent::setSettings(
    const IStringMap* settings)
{
    if (!logUtils.convertAndOutputStringMap(&m_settings, settings, "Received settings"))
        return error(ErrorCode::invalidParams, "Settings are invalid");

    return settingsReceived();
}

SettingsResponseResult VideoFrameProcessingDeviceAgent::pluginSideSettings() const
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

void VideoFrameProcessingDeviceAgent::pushPluginDiagnosticEvent(
    IPluginDiagnosticEvent::Level level,
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

    const auto event = makePtr<PluginDiagnosticEvent>(
        level, caption, description);
    m_handler->handlePluginDiagnosticEvent(event.get());
}

// TODO: Consider making a template with param type, checked according to the manifest.
std::string VideoFrameProcessingDeviceAgent::settingValue(const std::string& paramName)
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

void VideoFrameProcessingDeviceAgent::logMetadataPacketIfNeeded(
    const IMetadataPacket* metadataPacket,
    const std::string& packetIndexName) const
{
    if (!NX_DEBUG_ENABLE_OUTPUT)
        return;

    std::string packetName;
    if (queryInterfacePtr<const IObjectMetadataPacket>(metadataPacket))
    {
        packetName = "Object";
    }
    else if (queryInterfacePtr<const IEventMetadataPacket>(metadataPacket))
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

    auto compoundMetadataPacket = queryInterfacePtr<const ICompoundMetadataPacket>(metadataPacket);
    if (compoundMetadataPacket)
    {
        if (compoundMetadataPacket->count() == 0)
        {
            NX_OUTPUT << __func__ << "(): WARNING: " << packetName << " is empty.";
            return;
        }

        const std::string itemsName = (compoundMetadataPacket->count() == 1)
            ? (std::string("item of type ") + toPtr(compoundMetadataPacket->at(0))->typeId())
            : "item(s)";

        NX_OUTPUT << __func__ << "(): " << packetName << " contains "
            << compoundMetadataPacket->count() << " " << itemsName << ".";

        if (metadataPacket->timestampUs() == 0)
            NX_OUTPUT << __func__ << "(): WARNING: " << packetName << " has timestamp 0.";
    }
}

} // namespace analytics
} // namespace sdk
} // namespace nx
