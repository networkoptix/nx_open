#include "tegra_video.h"
#include "video_dec_gie_main.h"

#include <mutex>
#include <memory>

// Impl of TegraVideo.

#include <string>

#include "config.h"

#define OUTPUT_PREFIX "[tegra_video_impl #" << m_id << "] "
#include <nx/utils/debug_utils.h>
#include <nx/utils/string.h>

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
    void processRectsFromAdditionalDetectors();

    const std::string m_id;
    const std::string m_modelFile;
    const std::string m_deployFile;
    const std::string m_cacheFile;
    std::vector<std::unique_ptr<Detector>> m_detectors;
    std::map<int64_t, int64_t> m_ptsUsByFrameNumber;
    int64_t m_inFrameCounter;
    int64_t m_outFrameCounter;
    std::mutex m_mutex;
};

Impl::Impl(const Params& params):
    m_id(params.id),
    m_modelFile(params.modelFile),
    m_deployFile(params.deployFile),
    m_cacheFile(params.cacheFile),
    m_inFrameCounter(0),
    m_outFrameCounter(0)
{
    OUTPUT << "Impl() BEGIN";
    OUTPUT << "    id: " << m_id;
    OUTPUT << "    modelFile: " << m_modelFile;
    OUTPUT << "    deployFile: " << m_deployFile;
    OUTPUT << "    cacheFile: " << m_cacheFile;

    int decodersCount = conf.decodersCount;
    if (conf.decodersCount < 1)
    {
        PRINT << ".ini decodersCount should be not less than 1; assuming 1.";
        decodersCount = 1;
    }

    for (int i = 0; i < decodersCount; ++i)
    {
        m_detectors.emplace_back(new Detector(
            nx::kit::debug::format("%s.%d", m_id, i).c_str()));
        m_detectors.back()->startInference(m_modelFile, m_deployFile, m_cacheFile);
    }

    OUTPUT << "Impl() END";
}

Impl::~Impl()
{
    OUTPUT << "~Impl() BEGIN";

    for (auto& detector: m_detectors)
        detector->stopSync();

    OUTPUT << "~Impl() END";
}

bool Impl::pushCompressedFrame(const CompressedFrame* compressedFrame)
{
    OUTPUT << "pushCompressedFrame(data, dataSize: " << compressedFrame->dataSize
           << ", ptsUs: " << compressedFrame->ptsUs << ") BEGIN";

    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_inFrameCounter;

    for (auto& detector: m_detectors)
        detector->pushCompressedFrame(compressedFrame->data, compressedFrame->dataSize, compressedFrame->ptsUs);

    OUTPUT << "pushCompressedFrame() END -> true";
    return true;
}

/** Analyze rects from additional detectors (if any). */
void Impl::processRectsFromAdditionalDetectors()
{
    if (m_detectors.empty())
    {
        PRINT << "INTERNAL ERROR: No detectors.";
        return;
    }

    if (m_detectors.size() == 1)
        return; //< No need to log rects.

    for (int i = 0; i < m_detectors.size(); ++i)
    {
        /*OUTPUT << "Detector #" << i << ": "
            << (!m_detectors[i]->hasRectangles() ? 0 : m_detectors[i]->getRectangles().size())
            << " rects";*/
        int64_t ptsUs;
        if (m_detectors[i]->hasRectangles())
        {
            for (const auto& rect: m_detectors[i]->getRectangles(&ptsUs))
            {
                OUTPUT << "{ x " << rect.x << ", y " << rect.y
                    << ", width " << rect.width << ", height " << rect.height << " }, pts "
                    << ptsUs;
            }
        }
    }
}

bool Impl::pullRectsForFrame(
    Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs)
{
    OUTPUT << "pullRectsForFrame() BEGIN";
    if (!outRects || !outPtsUs)
        return false;

    processRectsFromAdditionalDetectors();

    if (!m_detectors.front()->hasRectangles())
    {
        OUTPUT << "pullRectsForFrame() END -> false: !m_detector->hasRectangles()";
        return false;
    }

    const auto rectsFromGie = m_detectors.front()->getRectangles(outPtsUs);
    const auto netHeight = m_detectors.front()->getNetHeight();
    const auto netWidth = m_detectors.front()->getNetWidth();

    if (rectsFromGie.size() > maxRectsCount)
    {
        PRINT << "INTERNAL ERROR: pullRectsForFrame(): too many rects: " << rectsFromGie.size();
        return false;
    }
    *outRectsCount = (int) rectsFromGie.size();

    for (int i = 0; i < rectsFromGie.size(); ++i)
    {
        const auto& rect = rectsFromGie[i];
        TegraVideo::Rect& r = outRects[i];
        r.x = (float) rect.x / netWidth;
        r.y = (float) rect.y / netHeight;
        r.width = (float) rect.width / netWidth;
        r.height = (float) rect.height / netHeight;
    }

    OUTPUT << "pullRectsForFrame() END";
    return true;
}

bool Impl::hasMetadata() const
{
    return m_detectors.front()->hasRectangles();
}

} // namespace

//-------------------------------------------------------------------------------------------------

TegraVideo* TegraVideo::createImpl(const Params& params)
{
    conf.reload();
    return new Impl(params);
}
