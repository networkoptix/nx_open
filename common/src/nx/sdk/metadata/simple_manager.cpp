#include "simple_manager.h"

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {
namespace metadata {

//-------------------------------------------------------------------------------------------------
// Implementations of interface methods.

// TODO: #mike: Decide on NX_PRINT_PREFIX to reflect actual plugin class, and change NX_PRINT to
// NX_OUTPUT, having decided on ini().

void* SimpleManager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_MetadataManager)
    {
        addRef();
        return static_cast<AbstractMetadataManager*>(this);
    }

    if (interfaceId == IID_ConsumingMetadataManager)
    {
        addRef();
        return static_cast<AbstractConsumingMetadataManager*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

Error SimpleManager::setHandler(AbstractMetadataHandler* handler)
{
    m_handler = handler;
    return Error::noError;
}

Error SimpleManager::putData(AbstractDataPacket* dataPacket)
{
    NX_PRINT << __func__ << "() BEGIN";

    nxpt::ScopedRef<CommonCompressedVideoPacket> videoFrame(
        dataPacket->queryInterface(IID_CompressedVideoPacket));
    if (!videoFrame)
        return Error::noError; //< No error: no video frame.

    if (!pushVideoFrame(videoFrame.get()))
    {
        NX_PRINT << __func__ << "() END -> unknownError: pushVideoFrame() failed";
        return Error::unknownError;
    }

    std::vector<AbstractMetadataPacket*> metadataPackets;
    if (!pullMetadataPackets(&metadataPackets))
    {
        NX_PRINT << __func__ << "() END -> unknownError: pullMetadataPackets() failed";
        return Error::unknownError;
    }

    if (!metadataPackets.empty())
    {
        NX_PRINT << __func__ << "() Producing " << metadataPackets.size()
            << " metadata packet(s)";
    }

    for (auto& metadataPacket: metadataPackets)
    {
        m_handler->handleMetadata(Error::noError, metadataPacket);
        metadataPacket->releaseRef();
    }

    NX_PRINT << __func__ << "() END -> noError";
    return Error::noError;
}

Error SimpleManager::startFetchingMetadata(
    nxpl::NX_GUID* /*eventTypeList*/, int /*eventTypeListSize*/)
{
    NX_PRINT << __func__ << "() -> noError";
    return Error::noError;
}

Error SimpleManager::stopFetchingMetadata()
{
    NX_PRINT << __func__ << "() -> noError";
    return Error::noError;
}

//-------------------------------------------------------------------------------------------------
// Tools for the derived class.

// TODO: #mike: Make a template with param type.
std::string SimpleManager::getParamValue(const char* paramName)
{
    if (!m_handler)
    {
        NX_PRINT << __func__ << "(): INTERNAL ERROR: Handler was not set.";
        return "";
    }

    static const int kInitialValueSize = 1000;
    std::string value;
    value.resize(kInitialValueSize);
    int size = value.size();
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
