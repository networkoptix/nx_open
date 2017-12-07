#include "tegra_video.h"

/**@file
 * Alternative implementation of libtegra_video.so (class TegraVideo) as a pure software stub.
 * */

#include <string>

#define NX_PRINT_PREFIX "[tegra_video_stub" << (m_id.empty() ? "" : " #" + m_id) << "] "
#include <nx/kit/debug.h>

#include "tegra_video_ini.h"

namespace {

class Stub: public TegraVideo
{
public:
    Stub()
    {
        NX_OUTPUT << __func__ << "()";
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

        m_hasMetadata = true;
        m_ptsUs = compressedFrame->ptsUs;
        return true;
    }

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) override
    {
        NX_OUTPUT << __func__ << "() -> false";

        *outPtsUs = m_ptsUs;

        // Produce 2 stub rects.
        if (maxRectsCount < 2)
        {
            NX_PRINT << __func__ << "(): ERROR: "
                << "maxRectsCount expected >= 2, actual " << maxRectsCount;
            return false;
        }
        *outRectsCount = 2;

        outRects[0].x = 0.1f;
        outRects[0].y = 0.2f;
        outRects[0].width = 0.7f;
        outRects[0].height = 0.5f;

        outRects[1].x = 0.2f;
        outRects[1].y = 0.1f;
        outRects[1].width = 0.4f;
        outRects[1].height = 0.8f;

        return true;
    }

    virtual bool hasMetadata() const override
    {
        return m_hasMetadata;
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
};

} // namespace

//-------------------------------------------------------------------------------------------------

TegraVideo* TegraVideo::createStub()
{
    ini().reload();
    return new Stub();
}
