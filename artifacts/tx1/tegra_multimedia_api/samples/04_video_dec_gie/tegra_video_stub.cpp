#include "tegra_video.h"

/**@file
 * Alternative implementation of libtegra_video.so (class TegraVideo) as a pure software stub.
 * */

#include <string>
#include <vector>
#include <algorithm>

#define NX_PRINT_PREFIX "[tegra_video_stub" << (m_id.empty() ? "" : " #" + m_id) << "] "
#include <nx/kit/debug.h>

#include "tegra_video_ini.h"

namespace {

struct PointF
{
    float x = 0;
    float y = 0;
};

class Stub: public TegraVideo
{
public:
    Stub()
    {
        NX_OUTPUT << __func__ << "()";

        initializeRectangles();
    }

    virtual ~Stub() override
    {
        NX_OUTPUT << __func__ << "()";
    }

    virtual bool start(const Params& params) override
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
        NX_OUTPUT << "})";

        m_modelFile = params.modelFile;
        m_deployFile = params.deployFile;
        m_cacheFile = params.cacheFile;
        m_netWidth = params.netWidth;
        m_netHeight = params.netHeight;

        return true;
    }

    virtual bool stop() override
    {
        NX_OUTPUT << __func__ << "() -> true";
        return true;
    }

    virtual bool pushCompressedFrame(const CompressedFrame* compressedFrame) override
    {
        NX_OUTPUT << __func__ << "(data, dataSize: " << compressedFrame->dataSize
            << ", ptsUs: " << compressedFrame->ptsUs << ") -> true";

        ++m_currentFrameIndex;
        if (ini().stubMetadataFrequency)
        {
            if (m_currentFrameIndex % ini().stubMetadataFrequency == 0)
            {
                m_hasMetadata = true;
                m_ptsUs = compressedFrame->ptsUs;
            }
        }   
        else
        {
            m_hasMetadata = true;
            m_ptsUs = compressedFrame->ptsUs;
        }

        return true;
    }

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) override
    {
        NX_OUTPUT << __func__ << "() -> true";

        *outPtsUs = m_ptsUs;
        makeRectangles(outRects, maxRectsCount, outRectsCount);
        m_hasMetadata = false;
        
        return true;
    }

    virtual bool hasMetadata() const override
    {
        return m_hasMetadata;
    }

private:
    void initializeRectangles()
    {
        const auto numberOfRectangles = ini().stubNumberOfRectangles;
        m_currentRectangleCenters.resize(numberOfRectangles);

        for (auto i = 0; i < numberOfRectangles; ++i)
        {
            PointF center;
            center.x = (i + 0.5f) * 1.0f / numberOfRectangles;
            center.y = 0;
            m_currentRectangleCenters[i] = center;
        }
    }

    void makeRectangles(Rect outRects[], int maxRectsCount, int* outRectsCount)
    {
        *outRectsCount = 0;
        const auto width = (float)ini().stubRectangleWidth / 100;
        const auto height = (float)ini().stubRectangleWidth / 100;
        const auto rectangleCount = std::min(ini().stubNumberOfRectangles, maxRectsCount);
        const int frequencyCoefficient = ini().stubMetadataFrequency ? ini().stubMetadataFrequency : 1;


        for (int i = 0; i < rectangleCount; ++i)
        {
            auto& center = m_currentRectangleCenters[i];
            center.y += 0.02 * frequencyCoefficient;
            if (center.y > 1)
                center.y = 0;

            outRects[i] = makeRectangleFromCenter(center);
            ++(*outRectsCount);
        }
    }

    Rect makeRectangleFromCenter(const PointF& center)
    {
        const auto width = (float)ini().stubRectangleWidth / 100;
        const auto height = (float)ini().stubRectangleWidth / 100;

        Rect rectangle;
        rectangle.x = std::max(0.0f, std::min(1.0f, center.x - width / 2));
        rectangle.y = std::max(0.0f, std::min(1.0f, center.y - height / 2));

        rectangle.width = std::min(1.0f - rectangle.x, width);
        rectangle.height = std::min(1.0f - rectangle.y, height);

        return rectangle;
    }

private:
    int64_t m_ptsUs;
    std::string m_id;
    std::string m_modelFile;
    std::string m_deployFile;
    std::string m_cacheFile;
    int m_netWidth = 0;
    int m_netHeight = 0;

    bool m_hasMetadata = false;
    int m_currentFrameIndex = 0;

    std::vector<PointF> m_currentRectangleCenters;
};

} // namespace

//-------------------------------------------------------------------------------------------------

TegraVideo* TegraVideo::createStub()
{
    ini().reload();
    return new Stub();
}
