#pragma once

#include <analytics/common/abstract_metadata_plugin.h>

namespace nx {
namespace analytics {

class VideoMetadataPlugin: public AbstractMetadataPlugin
{
public:
#if defined(ENABLE_DATA_PROVIDERS)
    virtual bool pushFrame(const CLVideoDecoderOutputPtr& decodedFrame) = 0;
#endif

    virtual bool pushFrame(const QnCompressedVideoDataPtr& compressedFrame) = 0;

    // TODO: #dmishin it's a temporary hack, remove it.
    virtual void setChannelNumber(int i) { m_channel = i; }

protected:
    int m_channel = 0;
};

} // analytics
} // nx
