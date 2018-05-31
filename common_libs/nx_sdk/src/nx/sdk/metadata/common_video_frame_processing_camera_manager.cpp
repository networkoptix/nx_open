#include "common_video_frame_processing_camera_manager.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#define NX_DEBUG_ENABLE_OUTPUT (this->utils.enableOutput)
#include <nx/kit/debug.h>

#include <nx/sdk/metadata/common_plugin.h>
#include <nx/sdk/utils.h>

namespace nx {
namespace sdk {
namespace metadata {

static std::string makePrintPrefix(Plugin* plugin)
{
    std::string base;
    if (const auto commonPlugin = dynamic_cast<CommonPlugin*>(plugin))
        base = commonPlugin->libName();
    else //< The plugin is not derived from CommonPlugin.
        base = plugin->name();

    return "[" + base + " camera] ";
}

CommonVideoFrameProcessingCameraManager::CommonVideoFrameProcessingCameraManager(
    Plugin* plugin,
    bool enableOutput,
    const std::string& printPrefix)
    :
    utils(enableOutput, !printPrefix.empty() ? printPrefix : makePrintPrefix(plugin)),
    m_plugin(plugin)
{
    NX_PRINT << "Created " << this;
}

CommonVideoFrameProcessingCameraManager::~CommonVideoFrameProcessingCameraManager()
{
    NX_PRINT << "Destroyed " << this;
}

//-------------------------------------------------------------------------------------------------
// Implementations of interface methods.

void* CommonVideoFrameProcessingCameraManager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_CameraManager)
    {
        addRef();
        return static_cast<CameraManager*>(this);
    }

    if (interfaceId == IID_ConsumingCameraManager)
    {
        addRef();
        return static_cast<ConsumingCameraManager*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

Error CommonVideoFrameProcessingCameraManager::setHandler(MetadataHandler* handler)
{
    m_handler = handler;
    return Error::noError;
}

Error CommonVideoFrameProcessingCameraManager::pushDataPacket(DataPacket* dataPacket)
{
    NX_OUTPUT << __func__ << "() BEGIN";

    if (!dataPacket)
    {
        NX_PRINT << __func__ << "() END -> unknownError: INTERNAL ERROR: dataPacket is null.";
        return Error::unknownError;
    }

    if (auto videoFrame = nxpt::ScopedRef<CompressedVideoPacket>(
        dataPacket->queryInterface(IID_CompressedVideoPacket)))
    {
        if (!pushCompressedVideoFrame(videoFrame.get()))
        {
            NX_OUTPUT << __func__ << "() END -> unknownError: pushCompressedVideoFrame() failed";
            return Error::unknownError;
        }
    }
    else if (auto videoFrame = nxpt::ScopedRef<UncompressedVideoFrame>(
        dataPacket->queryInterface(IID_UncompressedVideoFrame)))
    {
        if (!pushUncompressedVideoFrame(videoFrame.get()))
        {
            NX_OUTPUT << __func__ << "() END -> unknownError: pushUncompressedVideoFrame() failed";
            return Error::unknownError;
        }
    }
    else
    {
        NX_OUTPUT << __func__ << "() END -> noError: Unsupported frame supplied; ignored";
        return Error::noError;
    }

    std::vector<MetadataPacket*> metadataPackets;
    if (!pullMetadataPackets(&metadataPackets))
    {
        NX_OUTPUT << __func__ << "() END -> unknownError: pullMetadataPackets() failed";
        return Error::unknownError;
    }

    if (!metadataPackets.empty())
    {
        NX_OUTPUT << __func__ << "() Producing " << metadataPackets.size()
            << " metadata packet(s)";
    }

    for (auto& metadataPacket: metadataPackets)
    {
        if (metadataPacket)
        {
            m_handler->handleMetadata(Error::noError, metadataPacket);
            metadataPacket->releaseRef();
        }
        else
        {
            NX_OUTPUT << __func__ << "(): Null metadata packet found; ignored";
        }
    }

    NX_OUTPUT << __func__ << "() END -> noError";
    return Error::noError;
}

Error CommonVideoFrameProcessingCameraManager::startFetchingMetadata(
    nxpl::NX_GUID* /*typeList*/, int /*typeListSize*/)
{
    NX_PRINT << __func__ << "() -> noError";
    return Error::noError;
}

Error CommonVideoFrameProcessingCameraManager::stopFetchingMetadata()
{
    NX_PRINT << __func__ << "() -> noError";
    return Error::noError;
}

const char* CommonVideoFrameProcessingCameraManager::capabilitiesManifest(Error* /*error*/)
{
    const std::string manifest = capabilitiesManifest();
    char* data = new char[manifest.size() + 1];
    strncpy(data, manifest.c_str(), manifest.size() + 1);
    return data;
}

void CommonVideoFrameProcessingCameraManager::freeManifest(const char* data)
{
    delete[] data;
}

void CommonVideoFrameProcessingCameraManager::setDeclaredSettings(
    const nxpl::Setting* settings, int count)
{
    if (!utils.fillAndOutputSettingsMap(&m_settings, settings, count, "Received settings"))
        return; //< The error is already logged.

    settingsChanged();
}

//-------------------------------------------------------------------------------------------------
// Tools for the derived class.

void CommonVideoFrameProcessingCameraManager::pushMetadataPacket(
    sdk::metadata::MetadataPacket* metadataPacket)
{
    m_handler->handleMetadata(Error::noError, metadataPacket);
    metadataPacket->releaseRef();
}

// TODO: Consider making a template with param type, checked according to the manifest.
std::string CommonVideoFrameProcessingCameraManager::getParamValue(const char* paramName)
{
    return m_settings[paramName];
}

} // namespace metadata
} // namespace sdk
} // namespace nx
