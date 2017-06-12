#include "tegra_video.h"
#include "video_dec_gie_main.h"

#include <mutex>
#include <memory>

// Impl of TegraVideo.

#include <string>

#include "config.h"

#define OUTPUT_PREFIX "tegra_video<Impl>: "
#include <nx/utils/debug_utils.h>

namespace {

class Impl: public TegraVideo
{
public:
    Impl(const Params& params);

    virtual ~Impl() override;

    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) override;

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) override;

    virtual bool hasMetadata() const override;

private:
    const std::string m_modelFile;
    const std::string m_deployFile;
    const std::string m_cacheFile;
    std::unique_ptr<Detector> m_detector;
    std::map<int, int64_t> m_ptsMap;
    int64_t m_inFrameCounter;
    int64_t m_outFrameCounter;
    std::mutex m_mutex;

    // TODO: IMPLEMENT
};

Impl::Impl(const Params& params):
    m_modelFile(params.modelFile),
    m_deployFile(params.deployFile),
    m_cacheFile(params.cacheFile),
    m_detector(new Detector),
    m_inFrameCounter(0),
    m_outFrameCounter(0)
{
    OUTPUT << "Impl() BEGIN";
    OUTPUT << "    modelFile: " << m_modelFile;
    OUTPUT << "    deployFile: " << m_deployFile;
    OUTPUT << "    cacheFile: " << m_cacheFile;

    m_detector->startInference(m_modelFile, m_deployFile, m_cacheFile);

    OUTPUT << "Impl() END";
}

Impl::~Impl()
{
    OUTPUT << "~Impl() BEGIN";

    m_detector->stopSync();

    OUTPUT << "~Impl() END";
}

bool Impl::pushCompressedFrame(const CompressedFrame* compressedFrame)
{
    OUTPUT << "pushCompressedFrame(data, dataSize: " << compressedFrame->dataSize
           << ", ptsUs: " << compressedFrame->ptsUs << ") BEGIN";

    std::lock_guard<std::mutex> lock(m_mutex);
    m_ptsMap[m_inFrameCounter] = compressedFrame->ptsUs;
    ++m_inFrameCounter;

    m_detector->pushCompressedFrame(compressedFrame->data, compressedFrame->dataSize);

    OUTPUT << "pushCompressedFrame() END -> true";
    return true;
}

bool Impl::pullRectsForFrame(
    Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs)
{
    OUTPUT << "pullRectsForFrame() BEGIN";
    if (!rects || !outPtsUs)
        return false;

    if (!m_detector->hasRectangles())
    {
        OUTPUT << "pullRectsForFrame() END -> false: !m_detector->hasRectangles()";
        return false;
    }

    auto rectsFromGie = m_detector->getRectangles();
    auto netHeight = m_detector->getNetHeight();
    auto netWidth = m_detector->getNetWidth();

    if (rectsFromGie.size() > maxRectsCount)
    {
        PRINT << "INTERNAL ERROR: pullRectsForFrame(): too many rects: " << rectsFromGie.size();
        return false;
    }
    *outRectsCount = rectsFromGie.size();

    for (int i = 0; i < rectsFromGie.size(); ++i)
    {
        const auto& rect = rectsFromGie[i];
        TegraVideo::Rect& r = outRects[i];
        r.x = (float) rect.x / netWidth;
        r.y = (float) rect.y / netHeight;
        r.width = (float) rect.width / netWidth;
        r.height = (float) rect.height / netHeight;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        *outPtsUs = m_ptsMap[m_outFrameCounter];
        m_ptsMap.erase(m_outFrameCounter);
        ++m_outFrameCounter;
    }

    OUTPUT << "pullRectsForFrame() END";
    return true;
}

bool Impl::hasMetadata() const
{
    return m_detector->hasRectangles();
}

} // namespace

//-------------------------------------------------------------------------------------------------

TegraVideo* TegraVideo::createImpl(const Params& params)
{
    conf.reload();
    return new Impl(params);
}
