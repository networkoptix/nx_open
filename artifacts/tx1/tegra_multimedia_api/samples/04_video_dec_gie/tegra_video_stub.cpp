#include "tegra_video.h"

/**@file
 * Alternative implementation of libtegra_video.so (class TegraVideo) as a pure software stub.
 * */

#include <string>
#include <vector>
#include <algorithm>

#define NX_PRINT_PREFIX "[tegra_video_stub" << (m_id.empty() ? "" : " #" + m_id) << "] "
#include <nx/kit/debug.h>
#include <queue>

#include "tegra_video_ini.h"
#include "rects_serialization.h"

namespace {

struct PointF
{
    float x = 0;
    float y = 0;
};

class Stub final: public TegraVideo
{
public:
    Stub()
    {
        NX_OUTPUT << __func__ << "()";

        initializeRectangles();

        if (ini().rectanglesFilePrefix[0])
        {
            if (!readModulusFromFile(
                std::string(ini().rectanglesFilePrefix) + "modulus.txt", &m_ptsModulusUs))
            {
                m_ptsModulusUs = -1;
            }
            else
            {
                if (m_ptsModulusUs <= 0)
                {
                    NX_PRINT << "ERROR: Expected a positive integer in Modulus file, but "
                        << m_ptsModulusUs << " found.";
                }
            }
        }
    }

    virtual ~Stub() override
    {
        NX_OUTPUT << __func__ << "()";
    }

    virtual bool start(const Params* params) override
    {
        m_id = params->id; //< Used for logging, thus, assigned before logging.

        NX_OUTPUT << __func__ << "({";
        NX_OUTPUT << "    id: " << params->id;
        NX_OUTPUT << "    modelFile: " << params->modelFile;
        NX_OUTPUT << "    deployFile: " << params->deployFile;
        NX_OUTPUT << "    cacheFile: " << params->cacheFile;
        NX_OUTPUT << "    cacheFile: " << params->cacheFile;
        NX_OUTPUT << "    netWidth: " << params->netWidth;
        NX_OUTPUT << "    netHeight: " << params->netHeight;
        NX_OUTPUT << "})";

        m_modelFile = params->modelFile;
        m_deployFile = params->deployFile;
        m_cacheFile = params->cacheFile;
        m_netWidth = params->netWidth;
        m_netHeight = params->netHeight;

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
        if (ini().stubMetadataFrequency != 0)
        {
            if (m_currentFrameIndex % ini().stubMetadataFrequency != 0)
                return true;
        }

        m_hasMetadata = true;
        m_ptsUsQueue.push(compressedFrame->ptsUs);

        return true;
    }

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectCount, int* outRectCount, int64_t* outPtsUs) override
    {
        NX_OUTPUT << __func__ << "() -> true";

        *outPtsUs = m_ptsUsQueue.front();
        m_ptsUsQueue.pop();

        if (ini().rectanglesFilePrefix[0] && m_ptsModulusUs > 0)
        {
            const int64_t ptsModuloUs = *outPtsUs % m_ptsModulusUs;

            const std::string filename =
                ini().rectanglesFilePrefix + nx::kit::debug::format("%lld.txt", ptsModuloUs);
            if (!readRectsFromFile(filename, outRects, maxRectCount, outRectCount))
                return false; //< Error already logged.
        }
        else
        {
            makeRectangles(outRects, maxRectCount, outRectCount);
        }
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

    void makeRectangles(Rect outRects[], int maxRectCount, int* outRectCount)
    {
        *outRectCount = 0;
        const auto width = (float) ini().stubRectangleWidth / 100;
        const auto height = (float) ini().stubRectangleWidth / 100;
        const auto rectangleCount = std::min(ini().stubNumberOfRectangles, maxRectCount);
        const int frequencyCoefficient =
            ini().stubMetadataFrequency ? ini().stubMetadataFrequency : 1;


        for (int i = 0; i < rectangleCount; ++i)
        {
            auto& center = m_currentRectangleCenters[i];
            center.y += (float) 0.02 * frequencyCoefficient;
            if (center.y > 1)
                center.y = 0;

            outRects[i] = makeRectangleFromCenter(center);
            ++(*outRectCount);
        }
    }

    Rect makeRectangleFromCenter(const PointF& center)
    {
        const auto width = (float)ini().stubRectangleWidth / 100;
        const auto height = (float)ini().stubRectangleWidth / 100;

        Rect rectangle;
        rectangle.x = std::max(0.0f, std::min(1.0f, center.x - width / 2));
        rectangle.y = std::max(0.0f, std::min(1.0f, center.y - height / 2));

        rectangle.w = std::min(1.0f - rectangle.x, width);
        rectangle.h = std::min(1.0f - rectangle.y, height);

        return rectangle;
    }

private:
    std::queue<int64_t> m_ptsUsQueue;
    std::string m_id;
    std::string m_modelFile;
    std::string m_deployFile;
    std::string m_cacheFile;
    int m_netWidth = 0;
    int m_netHeight = 0;

    bool m_hasMetadata = false;
    int m_currentFrameIndex = 0;

    std::vector<PointF> m_currentRectangleCenters;

    int64_t m_ptsModulusUs = 0;
};

} // namespace

//-------------------------------------------------------------------------------------------------

TegraVideo* tegraVideoCreateStub()
{
    return new Stub();
}
