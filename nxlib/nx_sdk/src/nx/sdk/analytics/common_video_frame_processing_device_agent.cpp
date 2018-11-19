#include "common_video_frame_processing_device_agent.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->utils.enableOutput)
#include <nx/kit/debug.h>

#include <nx/sdk/utils.h>
#include <nx/sdk/common/string.h>
#include <nx/sdk/analytics/common_engine.h>
#include <nx/sdk/common/plugin_event.h>
#include <nx/sdk/analytics/plugin.h>

namespace nx {
namespace sdk {
namespace analytics {

class PrintPrefixMaker
{
public:
    std::string makePrintPrefix(const std::string& overridingPrintPrefix, const Engine* engine)
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
        const std::string printPrefix = "CommonVideoFrameProcessingDeviceAgent(): ";
    } utils;
};

CommonVideoFrameProcessingDeviceAgent::CommonVideoFrameProcessingDeviceAgent(
    Engine* engine,
    bool enableOutput,
    const std::string& printPrefix)
    :
    utils(enableOutput, PrintPrefixMaker().makePrintPrefix(printPrefix, engine)),
    m_engine(engine)
{
    NX_PRINT << "Created " << this;
}

CommonVideoFrameProcessingDeviceAgent::~CommonVideoFrameProcessingDeviceAgent()
{
    NX_PRINT << "Destroyed " << this;
}

//-------------------------------------------------------------------------------------------------
// Implementations of interface methods.

void* CommonVideoFrameProcessingDeviceAgent::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_DeviceAgent)
    {
        addRef();
        return static_cast<DeviceAgent*>(this);
    }

    if (interfaceId == IID_ConsumingDeviceAgent)
    {
        addRef();
        return static_cast<ConsumingDeviceAgent*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

Error CommonVideoFrameProcessingDeviceAgent::setHandler(DeviceAgent::IHandler* handler)
{
    std::scoped_lock lock(m_mutex);
    m_handler = handler;
    return Error::noError;
}

Error CommonVideoFrameProcessingDeviceAgent::pushDataPacket(DataPacket* dataPacket)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    if (!dataPacket)
    {
        NX_PRINT << __func__ << "() END -> unknownError: INTERNAL ERROR: dataPacket is null; "
            << "discarding the packet.";
        return Error::unknownError;
    }

    if (dataPacket->timestampUsec() < 0)
    {
        NX_PRINT << __func__ << "() END -> unknownError: INTERNAL ERROR: "
            << "dataPacket has invalid timestamp " << dataPacket->timestampUsec()
            << "; discarding the packet.";
        return Error::unknownError;
    }

    if (auto compressedFrame = nxpt::ScopedRef<CompressedVideoPacket>(
        dataPacket->queryInterface(IID_CompressedVideoPacket)))
    {
        if (!pushCompressedVideoFrame(compressedFrame.get()))
        {
            NX_OUTPUT << __func__ << "() END -> unknownError: pushCompressedVideoFrame() failed.";
            return Error::unknownError;
        }
    }
    else if (auto uncompressedFrame = nxpt::ScopedRef<UncompressedVideoFrame>(
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

    std::vector<MetadataPacket*> metadataPackets;
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
        std::scoped_lock lock(m_mutex);
        for (auto& metadataPacket: metadataPackets)
        {
            if (metadataPacket)
            {
                NX_KIT_ASSERT(metadataPacket->timestampUsec() >= 0);
                if (metadataPacket->timestampUsec() == 0)
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

Error CommonVideoFrameProcessingDeviceAgent::setNeededMetadataTypes(
    const IMetadataTypes* metadataTypes)
{
    if (metadataTypes->isNull())
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

Error CommonVideoFrameProcessingDeviceAgent::startFetchingMetadata(
    const IMetadataTypes* /*metadataTypes*/)
{
    NX_PRINT << __func__ << "() -> noError";
    return Error::noError;
}

void CommonVideoFrameProcessingDeviceAgent::stopFetchingMetadata()
{
    NX_PRINT << __func__ << "() -> noError";
}

const IString* CommonVideoFrameProcessingDeviceAgent::manifest(Error* /*error*/) const
{
    return new common::String(manifest());
}

void CommonVideoFrameProcessingDeviceAgent::setSettings(const nx::sdk::Settings* settings)
{
    if (!utils.fillAndOutputSettingsMap(&m_settings, settings, "Received settings"))
        return; //< The error is already logged.

    settingsChanged();
}

nx::sdk::Settings* CommonVideoFrameProcessingDeviceAgent::settings() const
{
    return nullptr;
}

//-------------------------------------------------------------------------------------------------
// Tools for the derived class.

void CommonVideoFrameProcessingDeviceAgent::pushMetadataPacket(
    nx::sdk::analytics::MetadataPacket* metadataPacket)
{
    if (!metadataPacket)
    {
        NX_PRINT << __func__ << "(): ERROR: metadataPacket is null; ignoring";
        return;
    }

    std::scoped_lock lock(m_mutex);
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

void CommonVideoFrameProcessingDeviceAgent::pushPluginEvent(
    IPluginEvent::Level level,
    std::string caption,
    std::string description)
{
    std::scoped_lock lock(m_mutex);
    if (!m_handler)
    {
        NX_PRINT << __func__ << "(): "
            << "INTERNAL ERROR: setHandler() was not called; ignoring plugin event";
        return;
    }

    nxpt::ScopedRef<IPluginEvent> event(
        new common::PluginEvent(level, caption, description),
        /*increaseRef*/ false);

    m_handler->handlePluginEvent(event.get());
}

// TODO: Consider making a template with param type, checked according to the manifest.
std::string CommonVideoFrameProcessingDeviceAgent::getParamValue(const char* paramName)
{
    return m_settings[paramName];
}

void CommonVideoFrameProcessingDeviceAgent::assertEngineCasted(void* engine) const
{
    NX_KIT_ASSERT(engine,
        "CommonVideoFrameProcessingDeviceAgent " + nx::kit::debug::toString(this)
        + " has m_engine of incorrect runtime type " + typeid(*m_engine).name());
}

} // namespace analytics
} // namespace sdk
} // namespace nx
