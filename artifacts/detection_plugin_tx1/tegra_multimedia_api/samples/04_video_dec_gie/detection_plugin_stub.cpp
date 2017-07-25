#include "../../../../detection_plugin_interface/detection_plugin_interface.h"

// Alternative impl of DetectionPlugin as a pure software stub.

#include <string>

#define NX_OUTPUT_PREFIX "[tegra_video_stub #" << m_id << "] "
#include <nx/kit/debug.h>

#include "config.h"

namespace {

class Stub: public AbstractDetectionPlugin
{
public:
    Stub(const Params& params);

    virtual ~Stub() override;

    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) override;

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) override;

    virtual bool hasMetadata() const override;

private:
    const std::string m_id;
    const std::string m_modelFile;
    const std::string m_deployFile;
    const std::string m_cacheFile;
};

Stub::Stub(const Params& params):
    m_id(params.id),
    m_modelFile(params.modelFile),
    m_deployFile(params.deployFile),
    m_cacheFile(params.cacheFile)
{
    NX_OUTPUT << "Stub(): created:";
    NX_OUTPUT << "    id: " << m_id;
    NX_OUTPUT << "    modelFile: " << m_modelFile;
    NX_OUTPUT << "    deployFile: " << m_deployFile;
    NX_OUTPUT << "    cacheFile: " << m_cacheFile;
}

Stub::~Stub()
{
    NX_OUTPUT << "~Stub(): destroyed.";
}

bool Stub::pushCompressedFrame(const CompressedFrame* compressedFrame)
{
    NX_OUTPUT << "pushCompressedFrame(data, dataSize: " << compressedFrame->dataSize
        << ", ptsUs: " << compressedFrame->ptsUs << ") -> true";
    return true;
}

bool Stub::pullRectsForFrame(
    Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs)
{
    NX_OUTPUT << "pullRectsForFrame() -> false";

    *outPtsUs = 0;
    *outRectsCount = 0;
    return false;
}

} // namespace

bool Stub::hasMetadata() const
{
    return false;
}

//-------------------------------------------------------------------------------------------------

AbstractDetectionPlugin* AbstractDetectionPlugin:createStub(const Params& params)
{
    ini().reload();
    return new Stub(params);
}
