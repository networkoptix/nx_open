#include "video_dec_gie_main.h"
#include "config.h"

#include <detection_plugin_interface/detection_plugin_interface.h>

#include <mutex>
#include <memory>
#include <string>

#define NX_OUTPUT_PREFIX "[tegra_video_impl #" << m_id << "] "
#include <nx/kit/debug.h>
#include <nx/kit/string.h>

namespace {
// Implemantation of AbstractDetectionPlugin interface for Jetson devices. 
class JetsonDetectionPlugin: public AbstractDetectionPlugin
{
public:
    JetsonDetectionPlugin();

    virtual ~JetsonDetectionPlugin() override;

    virtual void id(char* idStr, int maxIdStringLength) const override
    
    virtual bool hasMetadata() const override;
    
    virtual void setParams(const AbstractDetectionPlugin::Params&) override;
    
    virtual bool start() override;
    
    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) override;

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) override;

private:
    void processRectsFromAdditionalDetectors();

    const std::string m_id;
    const std::string m_modelFile;
    const std::string m_deployFile;
    const std::string m_cacheFile;
    std::vector<std::unique_ptr<Detector>> m_detectors;
    std::map<int64_t, int64_t> m_ptsUsByFrameNumber;
    int64_t m_inFrameCounter = 0;
    int64_t m_outFrameCounter = 0;
    std::mutex m_mutex;
};

JetsonDetectionPlugin::JetsonDetectionPlugin():
{
    NX_OUTPUT << "JetsonDetectionPlugin constructor has been called";
}

JetsonDetectionPlugin::~JetsonDetectionPlugin()
{
    NX_OUTPUT << "~JetsonDetectionPlugin() BEGIN";

    for (auto& detector: m_detectors)
        detector->stopSync();

    NX_OUTPUT << "~JetsonDetectionPlugin() END";
}

void id(char* idStr, int maxIdStringLength) const
{
    strncpy(idStr, m_id.c_str(), maxIdStringLength);
}

bool JetsonDetectionPlugin::hasMetadata() const
{
    return m_detectors.front()->hasRectangles();
}

void setParams(const AbstractDetectionPlugin::Params& params)
{
    m_id = params.id;
    m_deployFile = params.deployFile;
    m_modelFile = params.modelFile;
    m_cacheFile = params.cacheFile;
}

bool start()
{
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
        m_detectors.back()->startInference(m_modelFile, m_deployFile, m_cacheFile);
    }
    
    return true;
}

bool JetsonDetectionPlugin::pushCompressedFrame(const CompressedFrame* compressedFrame)
{
    NX_OUTPUT << "pushCompressedFrame(data, dataSize: " << compressedFrame->dataSize
           << ", ptsUs: " << compressedFrame->ptsUs << ") BEGIN";

    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_inFrameCounter;

    for (auto& detector: m_detectors)
        detector->pushCompressedFrame(compressedFrame->data, compressedFrame->dataSize, compressedFrame->ptsUs);

    NX_OUTPUT << "pushCompressedFrame() END -> true";
    return true;
}

/** Analyze rects from additional detectors (if any). */
void JetsonDetectionPlugin::processRectsFromAdditionalDetectors()
{
    if (m_detectors.empty())
    {
        NX_PRINT << "INTERNAL ERROR: No detectors.";
        return;
    }

    if (m_detectors.size() == 1)
        return; //< No need to log rects.

    for (int i = 0; i < m_detectors.size(); ++i)
    {
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

bool JetsonDetectionPlugin::pullRectsForFrame(
    Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs)
{
    NX_OUTPUT << "pullRectsForFrame() BEGIN";
    if (!outRects || !outPtsUs)
        return false;

    processRectsFromAdditionalDetectors();

    if (!m_detectors.front()->hasRectangles())
    {
        NX_OUTPUT << "pullRectsForFrame() END -> false: !m_detector->hasRectangles()";
        return false;
    }

    const auto rectsFromGie = m_detectors.front()->getRectangles(outPtsUs);
    const auto netHeight = m_detectors.front()->getNetHeight();
    const auto netWidth = m_detectors.front()->getNetWidth();

    if (rectsFromGie.size() > maxRectsCount)
    {
        NX_PRINT << "INTERNAL ERROR: pullRectsForFrame(): too many rects: " << rectsFromGie.size();
        return false;
    }
    
    *outRectsCount = (int) rectsFromGie.size();

    for (int i = 0; i < rectsFromGie.size(); ++i)
    {
        const auto& rect = rectsFromGie[i];
        AbstractDetectionPlugin::Rect& r = outRects[i];
        r.x = (float) rect.x / netWidth;
        r.y = (float) rect.y / netHeight;
        r.width = (float) rect.width / netWidth;
        r.height = (float) rect.height / netHeight;
    }

    NX_OUTPUT << "pullRectsForFrame() END";
    return true;
}

} // namespace

//-------------------------------------------------------------------------------------------------

extern "C" {

AbstractDetectionPlugin* createDetectionPluginInstance()
{
    return new JetsonDetectionPlugin();
}

} // extern "C"
