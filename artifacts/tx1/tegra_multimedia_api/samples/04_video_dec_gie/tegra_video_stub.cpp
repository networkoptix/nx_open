#include "tegra_video.h"
/**@file Alternative implementation of TegraVideo as a pure software stub. */

#include <string>

#define NX_PRINT_PREFIX "[tegra_video_stub #" << m_id << "] "
#include <nx/kit/debug.h>

#include "tegra_video_ini.h"

namespace {

class Stub: public TegraVideo
{
public:
    Stub()
    {
        NX_OUTPUT << "Stub(): created";
    }

    virtual ~Stub() override
    {
        NX_OUTPUT << "~Stub(): destroyed";
    }

    virtual bool start(const Params& params) override
    {
        NX_OUTPUT << "Stub(): params:";
        NX_OUTPUT << "{";
        NX_OUTPUT << "    id: " << params.id;
        NX_OUTPUT << "    modelFile: " << params.modelFile;
        NX_OUTPUT << "    deployFile: " << params.deployFile;
        NX_OUTPUT << "    cacheFile: " << params.cacheFile;
        NX_OUTPUT << "    cacheFile: " << params.cacheFile;
        NX_OUTPUT << "    netWidth: " << params.netWidth;
        NX_OUTPUT << "    netHeight: " << params.netHeight;
        NX_OUTPUT << "}";

        m_id = params.id;
        m_modelFile = params.modelFile;
        m_deployFile = params.deployFile;
        m_cacheFile = params.cacheFile;
        m_netWidth = params.netWidth;
        m_netHeight = params.netHeight;
    }

    bool pushCompressedFrame(const CompressedFrame* compressedFrame) override
    {
        NX_OUTPUT << "pushCompressedFrame(data, dataSize: " << compressedFrame->dataSize
                  << ", ptsUs: " << compressedFrame->ptsUs << ") -> true";
        return true;
    }

    virtual bool pullRectsForFrame(
        Rect outRects[], int maxRectsCount, int* outRectsCount, int64_t* outPtsUs) override
    {
        NX_OUTPUT << "pullRectsForFrame() -> false";

        *outPtsUs = 0;
        *outRectsCount = 0;
        return false;
    }

    virtual bool hasMetadata() const override
    {
        return false;
    }

private:
    std::string m_id;
    std::string m_modelFile;
    std::string m_deployFile;
    std::string m_cacheFile;
    int m_netWidth = 0;
    int m_netHeight = 0;
};

} // namespace

//-------------------------------------------------------------------------------------------------

TegraVideo* TegraVideo::createStub()
{
    ini().reload();
    return new Stub();
}
