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
#include "rects_serialization.h"

#define NX_PRINT_PREFIX "[tegra_video_impl" << (m_id.empty() ? "" : " #" + m_id) << "] "
#include <nx/kit/debug.h>

namespace {

class Impl final: public TegraVideo
{
public:
    Impl();
    ~Impl();

    virtual bool start(const Params* params) override;

    virtual bool stop() override;

    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) override;

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectCount, int* outRectCount, int64_t* outPtsUs) override;

    virtual bool hasMetadata() const override;

private:
    bool processRectsFromAdditionalDetectors();
    void updateMinAndMaxPts(int64_t ptsUs);
    void writeRects(const Rect rects[], int rectCount, int64_t ptsUs);

    std::string m_id;
    std::string m_modelFile;
    std::string m_deployFile;
    std::string m_cacheFile;
    int m_netWidth = 0;
    int m_netHeight = 0;

    std::vector<std::unique_ptr<Detector>> m_detectors;
    int64_t m_inFrameCounter = 0;
    std::mutex m_mutex;
    int64_t m_prevPtsUs = 0;
    int64_t m_minPtsUs = INT64_MAX;
    int64_t m_maxPtsUs = INT64_MIN;
    int64_t m_incrementOfMaxPts = 0;
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

bool Impl::start(const Params* params)
{
    m_id = params->id; //< Used for logging, thus, assigned before logging.

    NX_OUTPUT << __func__ << "({";
    NX_OUTPUT << "    id: " << params->id;
    NX_OUTPUT << "    modelFile: " << params->modelFile;
    NX_OUTPUT << "    deployFile: " << params->deployFile;
    NX_OUTPUT << "    cacheFile: " << params->cacheFile;
    NX_OUTPUT << "    netWidth: " << params->netWidth;
    NX_OUTPUT << "    netHeight: " << params->netHeight;
    NX_OUTPUT << "}) BEGIN";

    m_modelFile = params->modelFile;
    m_deployFile = params->deployFile;
    m_cacheFile = params->cacheFile;

    auto fileStream = std::ifstream(m_deployFile);
    const NetDimensions dimensions = getNetDimensions(
        fileStream, m_deployFile, {params->netWidth, params->netHeight});
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
bool Impl::processRectsFromAdditionalDetectors()
{
    if (m_detectors.empty())
    {
        NX_PRINT << "INTERNAL ERROR: " << __func__ << "(): No detectors.";
        return false;
    }

    if (m_detectors.size() == 1)
        return true; //< No need to log rects.

    for (int i = 0; i < m_detectors.size(); ++i)
    {
        NX_OUTPUT << "Detector #" << i << ": "
            << (
                !m_detectors[i]->hasRectangles()
                ? 0
                : m_detectors[i]->getRectangles(nullptr).size()
            ) << " rects";

        int64_t ptsUs;
        if (m_detectors[i]->hasRectangles())
        {
            for (const auto& rect: m_detectors[i]->getRectangles(&ptsUs))
            {
                NX_OUTPUT << "{ x " << rect.x << ", y " << rect.y
                    << ", w " << rect.width << ", h " << rect.height << " }, pts "
                    << ptsUs;
            }
        }
    }

    return true;
}

bool Impl::pullRectsForFrame(
    Rect outRects[], int maxRectCount, int* const outRectCount, int64_t* const outPtsUs)
{
    NX_OUTPUT << __func__ << "() BEGIN";
    if (!outRects || !outPtsUs)
    {
        NX_OUTPUT << __func__ << "() END -> false: either outRects or outPtsUs is null";
        return false;
    }

    if (!processRectsFromAdditionalDetectors())
        return false;

    if (!m_detectors.front()->hasRectangles())
    {
        NX_OUTPUT << __func__ << "() END -> false: !m_detector->hasRectangles()";
        return false;
    }

    const auto rectsFromGie = m_detectors.front()->getRectangles(outPtsUs);

    updateMinAndMaxPts(*outPtsUs);

    if (rectsFromGie.size() > maxRectCount)
    {
        NX_PRINT << "INTERNAL ERROR: "
            << __func__ << "(): Too many rects: " << rectsFromGie.size();
        return false;
    }

    *outRectCount = (int) rectsFromGie.size();

    for (int i = 0; i < rectsFromGie.size(); ++i)
    {
        const auto& rect = rectsFromGie[i];
        TegraVideo::Rect& r = outRects[i];
        r.x = (float) rect.x / m_netWidth;
        r.y = (float) rect.y / m_netHeight;
        r.w = (float) rect.width / m_netWidth;
        r.h = (float) rect.height / m_netHeight;
    }

    if (ini().rectanglesFilePrefix[0])
        writeRects(outRects, *outRectCount, *outPtsUs);

    NX_OUTPUT << __func__ << "() END -> true";
    return true;
}

void Impl::updateMinAndMaxPts(int64_t ptsUs)
{
    if (ptsUs > m_maxPtsUs)
    {
        m_maxPtsUs = ptsUs;
        m_incrementOfMaxPts = m_maxPtsUs - m_prevPtsUs;
    }
    m_minPtsUs = std::min(ptsUs, m_minPtsUs);
    m_prevPtsUs = ptsUs;
}

void Impl::writeRects(const Rect outRects[], int rectCount, int64_t ptsUs)
{
    const bool noPrevFile = writeRectsToFile(
        ini().rectanglesFilePrefix + nx::kit::debug::format("%lld.txt", ptsUs),
        outRects, rectCount);

    if (noPrevFile)
        return;
    // A frame with the same pts was already saved - detect looping and save the modulus.

    // Assume the interval between the last and the first frame of the loop equal to the interval
    // between the frame with the maximum pts and its previous frame.
    const int64_t ptsModulusUs = m_maxPtsUs - m_minPtsUs + m_incrementOfMaxPts;

    writeModulusToFile(std::string(ini().rectanglesFilePrefix) + "modulus.txt", ptsModulusUs);
    // Ignore the returned value - error already logged.
}

bool Impl::hasMetadata() const
{
    NX_OUTPUT << __func__ << "() BEGIN";

    if (m_detectors.empty())
    {
        NX_PRINT << "INTERNAL ERROR: " << __func__ << "(): No detectors.";
        NX_OUTPUT << __func__ << "() END -> false";
        return false;
    }

    const bool result = m_detectors.front()->hasRectangles();
    NX_OUTPUT << __func__ << "() END -> " << (result ? "true" : "false");
    return result;
}

} // namespace

//-------------------------------------------------------------------------------------------------

TegraVideo* tegraVideoCreateImpl()
{
    return new Impl();
}
