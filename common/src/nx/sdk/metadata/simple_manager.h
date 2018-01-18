#pragma once

#include <string>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_consuming_metadata_manager.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Base class for a typical implementation of the metadata manager. Hides many technical details of
 * the Metadata Plugin SDK, but may limit manager capabilities - use only when suitable.
 */
class SimpleManager: public nxpt::CommonRefCounter<AbstractConsumingMetadataManager>
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual Error setHandler(AbstractMetadataHandler* handler) override;

    virtual Error putData(AbstractDataPacket* dataPacket) override;

protected:
    /**
     * Accept next video frame for processing.
     */
    virtual bool pushVideoFrame(const CommonCompressedVideoPacket* videoFrame) = 0;

    /**
     * Retrieve constructed metadata packets (if any) to the given list.
     */
    virtual bool pullMetadataPackets(std::vector<AbstractMetadataPacket*>* metadataPackets) = 0;

    /**
     * @return Param value, or an empty string if such param does not exist, having logged the
     * error.
     */
    std::string getParamValue(const char* paramName);

protected:
    mutable std::mutex mutex;

private:
    AbstractMetadataHandler* m_handler = nullptr;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
