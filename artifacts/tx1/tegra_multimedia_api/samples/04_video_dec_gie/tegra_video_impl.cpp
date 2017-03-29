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

    virtual bool pullRectsForFrame(std::vector<Rect>* rects, int64_t* outPtsUs) override;

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

    return true;
}

bool Impl::pullRectsForFrame(std::vector<Rect>* rects, int64_t* outPtsUs)
{

    if (!rects || !outPtsUs)
        return false;

    if (!m_detector->hasRectangles())
        return false;

    auto rectsFromGie = m_detector->getRectangles();
    auto netHeight = m_detector->getNetHeight();
    auto netWidth = m_detector->getNetWidth();

    for (const auto& rect: rectsFromGie)
    {
        TegraVideo::Rect r;
        r.x = (float) rect.x / netWidth;
        r.y = (float) rect.y / netHeight;
        r.width = (float) rect.width / netWidth;
        r.height = (float) rect.height / netHeight;

        rects->push_back(r);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        *outPtsUs = m_ptsMap[m_outFrameCounter];
        m_ptsMap.erase(m_outFrameCounter);
        ++m_outFrameCounter;
    }

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
