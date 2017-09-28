#include "tegra_video.h"

// Alternative impl of TegraVideo as a pure software stub.

#include <string>

#define OUTPUT_PREFIX "[tegra_video_stub #" << m_id << "] "
#include <nx/utils/debug_utils.h>

#include "config.h"

namespace {

class Stub: public TegraVideo
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
    OUTPUT << "Stub(): created:";
    OUTPUT << "    id: " << m_id;
    OUTPUT << "    modelFile: " << m_modelFile;
    OUTPUT << "    deployFile: " << m_deployFile;
    OUTPUT << "    cacheFile: " << m_cacheFile;
}

Stub::~Stub()
{
    OUTPUT << "~Stub(): destroyed.";
}

bool Stub::pushCompressedFrame(const CompressedFrame* compressedFrame)
{
    OUTPUT << "pushCompressedFrame(data, dataSize: " << compressedFrame->dataSize
        << ", ptsUs: " << compressedFrame->ptsUs << ") -> true";
    return true;
}

bool Stub::pullRectsForFrame(
    Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs)
{
    OUTPUT << "pullRectsForFrame() -> false";

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

TegraVideo* TegraVideo::createStub(const Params& params)
{
    conf.reload();
    return new Stub(params);
}
