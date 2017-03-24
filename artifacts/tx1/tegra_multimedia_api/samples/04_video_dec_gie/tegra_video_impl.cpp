#include "tegra_video.h"

// Impl of TegraVideo.

#include <string>

#define OUTPUT_PREFIX "tegra_video<Impl>: "
#include <nx/utils/debug_utils.h>

#include "config.h"

namespace {

class Impl: public TegraVideo
{
public:
    Impl(const Params& params);

    virtual ~Impl() override;

    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) override;

    virtual bool pullRectsForFrame(std::vector<Rect>* rects, int64_t* outPtsUs) override;

private:
    const std::string m_modelFile;
    const std::string m_deployFile;
    const std::string m_cacheFile;

    // TODO: IMPLEMENT
};

Impl::Impl(const Params& params):
    m_modelFile(params.modelFile),
    m_deployFile(params.deployFile),
    m_cacheFile(params.cacheFile)
{
    OUTPUT << "Impl() BEGIN";
    OUTPUT << "    modelFile: " << m_modelFile;
    OUTPUT << "    deployFile: " << m_deployFile;
    OUTPUT << "    cacheFile: " << m_cacheFile;

    // TODO: IMPLEMENT

    OUTPUT << "Impl() END";
}

Impl::~Impl()
{
    OUTPUT << "~Impl() BEGIN";

    // TODO: IMPLEMENT

    OUTPUT << "~Impl() END";
}

bool Impl::pushCompressedFrame(const CompressedFrame* compressedFrame)
{
    OUTPUT << "pushCompressedFrame(data, dataSize: " << compressedFrame->dataSize
           << ", ptsUs: " << compressedFrame->ptsUs << ") BEGIN";

    // TODO: IMPLEMENT

    OUTPUT << "pushCompressedFrame() END";
}

bool Impl::pullRectsForFrame(std::vector<Rect>* rects, int64_t* outPtsUs)
{
    OUTPUT << "pullRectsForFrame() BEGIN";

    // TODO: IMPLEMENT
    *outPtsUs = 0;

    OUTPUT << "pullRectsForFrame() END";
}

} // namespace

//-------------------------------------------------------------------------------------------------

TegraVideo* TegraVideo::createImpl(const Params& params)
{
    conf.reload();
    return new Impl(params);
}
