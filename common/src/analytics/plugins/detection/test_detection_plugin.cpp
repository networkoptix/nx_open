#include "test_detection_plugin.h"

#include <chrono>

namespace nx {
namespace analytics {

namespace {

const char kId[] = "TestDetectionPlugin";

} // namespace

void TestDetectionPlugin::id(char* idString, int maxIdStringLength) const
{
    strncpy(idString, kId, sizeof(kId));
}

void TestDetectionPlugin::setParams(const AbstractDetectionPlugin::Params& params)
{
    // Do nothing.
}

bool TestDetectionPlugin::hasMetadata() const
{
    return m_hasMetadata;
}

bool TestDetectionPlugin::start()
{
    return true;
}

bool TestDetectionPlugin::pushCompressedFrame(
    const AbstractDetectionPlugin::CompressedFrame* compressedFrame)
{
    m_hasMetadata = true;
    return true;
}

bool TestDetectionPlugin::pullRectsForFrame(
    AbstractDetectionPlugin::Rect outRects[],
    int maxRectsCount,
    int* outRectsCount,
    int64_t* outPtsUs)
{
    m_hasMetadata = false;
    if (!maxRectsCount)
        return true;

    AbstractDetectionPlugin::Rect rect;

    rect.x = 0.2;
    rect.y = 0.2;
    rect.height = 0.6;
    rect.width = 0.6;

    outRects[0] = rect;
    *outRectsCount = 1;

    using namespace std::chrono;
    auto currentTimePoint = high_resolution_clock::now();
    *outPtsUs = duration_cast<microseconds>(currentTimePoint.time_since_epoch()).count();

    return true;
}

} // namespace analytics
} // namespace nx
