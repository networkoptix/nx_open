#include "tegra_video.h"

/**@file
 * Implementation of libtegra_video.so (class TegraVideo).
 */

#include "video_dec_gie_main.h"

#include <mutex>
#include <memory>
#include <string>

#include "tegra_video_ini.h"
#include "net_dimensions.h"

#define NX_PRINT_PREFIX "[tegra_video_impl" << (m_id.empty() ? "" : " #" + m_id) << "] "
#include <nx/kit/debug.h>

namespace {

class Impl: public TegraVideo
{
public:
    Impl();

    virtual ~Impl() override;

    virtual bool start(const Params& params) override;

    virtual bool stop() override;

    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) override;

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) override;

    virtual bool hasMetadata() const override;

private:
    void processRectsFromAdditionalDetectors();

    std::string m_id;
    std::string m_modelFile;
    std::string m_deployFile;
    std::string m_cacheFile;
    int m_netWidth = 0;
    int m_netHeight = 0;

    std::vector<std::unique_ptr<Detector>> m_detectors;
    int64_t m_inFrameCounter = 0;
    std::mutex m_mutex;
};

Impl::Impl()
{
    NX_OUTPUT << __func__ << "()";
}

Impl::~Impl()
{
    NX_OUTPUT << __func__ << "() BEGIN";

    stop(); //< Ignore possible errors.

    NX_OUTPUT << __func__ << "() END";
}

bool Impl::start(const Params& params)
{
    m_id = params.id; //< Used for logging, thus, assigned before logging.

    NX_OUTPUT << __func__ << "({";
    NX_OUTPUT << "    id: " << params.id;
    NX_OUTPUT << "    modelFile: " << params.modelFile;
    NX_OUTPUT << "    deployFile: " << params.deployFile;
    NX_OUTPUT << "    cacheFile: " << params.cacheFile;
    NX_OUTPUT << "    cacheFile: " << params.cacheFile;
    NX_OUTPUT << "    netWidth: " << params.netWidth;
    NX_OUTPUT << "    netHeight: " << params.netHeight;
    NX_OUTPUT << "}) BEGIN";

    m_modelFile = params.modelFile;
    m_deployFile = params.deployFile;
    m_cacheFile = params.cacheFile;

    auto fileStream = std::ifstream(m_deployFile);
    const NetDimensions dimensions = getNetDimensions(
        fileStream, m_deployFile, {params.netWidth, params.netHeight});
    if (dimensions.isNull())
        return false;
    m_netWidth = dimensions.width;
    m_netHeight = dimensions.height;

    int decodersCount = ini().decodersCount;
    if (ini().decodersCount < 1)
    {
        NX_PRINT << ".ini decodersCount should be not less than 1; assuming 1.";
        decodersCount = 1;
    }

    for (int i = 0; i < decodersCount; ++i)
    {
        m_detectors.emplace_back(new Detector(
            nx::kit::debug::format("%s.%d", m_id, i).c_str()));

        const int result = m_detectors.back()->startInference(
            m_modelFile, m_deployFile, m_cacheFile);
        if (result != 0)
        {
            NX_OUTPUT << __func__
                << "() END -> false: Detector::startInference() failed with error " << result;
            return false;
        }
    }

    NX_OUTPUT << __func__ << "() END -> true";
    return true;
}

bool Impl::stop()
{
    NX_OUTPUT << __func__ << "() BEGIN";

    bool success = true;

    for (auto& detector: m_detectors)
    {
        if (!detector->stopSync())
            success = false;
        detector.reset();
    }
    m_detectors.clear();

    NX_OUTPUT << __func__ << "() END -> " << (success ? "true" : "false");
    return success;
}

bool Impl::pushCompressedFrame(const CompressedFrame* compressedFrame)
{
    NX_OUTPUT << __func__ << "(data, dataSize: " << compressedFrame->dataSize
           << ", ptsUs: " << compressedFrame->ptsUs << ") BEGIN";

    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_inFrameCounter;

    for (auto& detector: m_detectors)
    {
        detector->pushCompressedFrame(
            compressedFrame->data, compressedFrame->dataSize, compressedFrame->ptsUs);
    }

    NX_OUTPUT << __func__ << "() END -> true";
    return true;
}

/** Analyze rects from additional detectors (if any). */
void Impl::processRectsFromAdditionalDetectors()
{
    if (m_detectors.empty())
    {
        NX_PRINT << "INTERNAL ERROR: " << __func__ << "(): No detectors.";
        return;
    }

    if (m_detectors.size() == 1)
        return; //< No need to log rects.

    for (int i = 0; i < m_detectors.size(); ++i)
    {
#if 0
        NX_OUTPUT << "Detector #" << i << ": "
            << (!m_detectors[i]->hasRectangles() ? 0 : m_detectors[i]->getRectangles().size())
            << " rects";
#endif // 0
        int64_t ptsUs;
        if (m_detectors[i]->hasRectangles())
        {
            for (const auto& rect: m_detectors[i]->getRectangles(&ptsUs))
            {
                NX_OUTPUT << "{ x " << rect.x << ", y " << rect.y
                    << ", width " << rect.width << ", height " << rect.height << " }, pts "
                    << ptsUs;
            }
        }
    }
}

bool Impl::pullRectsForFrame(
    Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs)
{
    NX_OUTPUT << __func__ << "() BEGIN";
    if (!outRects || !outPtsUs)
    {
        NX_OUTPUT << __func__ << "() END -> false: either outRects or outPtsUs is null";
        return false;
    }

    processRectsFromAdditionalDetectors();

    if (!m_detectors.front()->hasRectangles())
    {
        NX_OUTPUT << __func__ << "() END -> false: !m_detector->hasRectangles()";
        return false;
    }

    const auto rectsFromGie = m_detectors.front()->getRectangles(outPtsUs);

    if (rectsFromGie.size() > maxRectsCount)
    {
        NX_PRINT << "INTERNAL ERROR: "
            << __func__ << "(): Too many rects: " << rectsFromGie.size();
        return false;
    }

    *outRectsCount = (int) rectsFromGie.size();

    for (int i = 0; i < rectsFromGie.size(); ++i)
    {
        const auto& rect = rectsFromGie[i];
        TegraVideo::Rect& r = outRects[i];
        r.x = (float) rect.x / m_netWidth;
        r.y = (float) rect.y / m_netHeight;
        r.width = (float) rect.width / m_netWidth;
        r.height = (float) rect.height / m_netHeight;
    }

    NX_OUTPUT << __func__ << "() END -> true";
    return true;
}

bool Impl::hasMetadata() const
{
    return m_detectors.front()->hasRectangles();
}

} // namespace

//-------------------------------------------------------------------------------------------------

#if !defined(TEGRA_VIDEO_STUB_ONLY)

TegraVideo* TegraVideo::createImpl()
{
    ini().reload();
    return new Impl();
}

#endif // !defined(TEGRA_VIDEO_STUB_ONLY)
