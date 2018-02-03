#include "common_video_frame_processing_camera_manager.h"

#define NX_DEBUG_ENABLE_OUTPUT m_enableOutput
#define NX_PRINT_PREFIX (std::string("[") + this->plugin()->name() + " CameraManager] ")
#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace metadata {

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

    nxpt::ScopedRef<CommonCompressedVideoPacket> videoFrame(
        dataPacket->queryInterface(IID_CompressedVideoPacket));
    if (!videoFrame)
        return Error::noError; //< No error: no video frame.

    if (!pushVideoFrame(videoFrame.get()))
    {
        NX_OUTPUT << __func__ << "() END -> unknownError: pushVideoFrame() failed";
        return Error::unknownError;
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
        m_handler->handleMetadata(Error::noError, metadataPacket);
        metadataPacket->releaseRef();
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
    if (!m_handler)
    {
        NX_PRINT << __func__ << "(): INTERNAL ERROR: Handler was not set.";
        return "";
    }

    static const int kInitialValueSize = 1000;
    std::string value;
    value.resize(kInitialValueSize);
    int size = (int) value.size();
    switch (m_handler->getParamValue(paramName, &value[0], &size))
    {
        case NX_UNKNOWN_PARAMETER:
            NX_PRINT << "ERROR: Manager requested unknown settings param [" << paramName << "].";
            return "";

        case NX_MORE_DATA:
        {
            value.resize(size);
            auto r =  m_handler->getParamValue(paramName, &value[0], &size);
            if (r != NX_NO_ERROR)
            {
                NX_PRINT << "INTERNAL ERROR: m_handler->getParamValue() failed (" << r
                    << ") when supplied the previously returned size " << size
                    << " for param [" << paramName << "].";
                return "";
            }
            break;
        }

        case NX_NO_ERROR:
            break;

        default:
            NX_PRINT << __func__ << "INTERNAL ERROR";
            return "";
    }

    value.resize(strlen(value.c_str())); //< Strip unused allocated bytes.
    return value;
}

} // namespace metadata
} // namespace sdk
} // namespace nx
