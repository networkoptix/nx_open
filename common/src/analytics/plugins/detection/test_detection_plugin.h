#pragma once

#include <detection_plugin_interface/detection_plugin_interface.h>

namespace nx {
namespace analytics {

class TestDetectionPlugin: public AbstractDetectionPlugin
{
    virtual void id (char* idString, int maxIdStringLength) const override;
    virtual void setParams(const AbstractDetectionPlugin::Params& params) override;
    virtual bool hasMetadata() const override;

    virtual bool start() override;

    virtual bool pushCompressedFrame(const AbstractDetectionPlugin::CompressedFrame* compressedFrame);
    virtual bool pullRectsForFrame(
        AbstractDetectionPlugin::Rect outRects[],
        int maxRectsCount,
        int* outRectsCount,
        int64_t* outPtsUs);

private:
    mutable bool m_hasMetadata = true;

};

} // namespace analytics
} // namespace nx