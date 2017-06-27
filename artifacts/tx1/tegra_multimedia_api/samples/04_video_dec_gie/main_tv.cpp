#include "main_tv.h"

#include <iostream>
#include <fstream>
#include <ios>
#include <memory>

#include "tegra_video.h"

#include "config.h"

#define OUTPUT_PREFIX "[video_dec_gie main_tv] "
#include <nx/utils/debug_utils.h>

#include <nx/utils/string.h>

namespace {

static constexpr int kChunkSize = 4000000;

static int loadFile(const std::string& filename, uint8_t* buf, int maxSize)
{
    std::ifstream file;
    file.open(filename, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Can't open input file " << filename << std::endl;
        return -1;
    }

    int dataSize;

    file.read((char*) buf, maxSize);
    dataSize = file.gcount();
    if (file.bad())
    {
        PRINT << "ERROR: Unable to read file.";
        return -1;
    }

    if (dataSize < 0)
    {
        PRINT << "ERROR: file.read() < 0.";
        return -1;
    }

    OUTPUT << "Substituted frame from file: " << filename;
    return dataSize;
}

} // namespace

// TODO: Remove TEGRA_VIDEO_API
int TEGRA_VIDEO_API mainTv(int argc, char** argv)
{
    OUTPUT << "mainTv()";

    TegraVideo::Params params;
    params.modelFile = conf.modelFile;
    params.deployFile = conf.deployFile;
    params.cacheFile = conf.cacheFile;
    std::unique_ptr<TegraVideo> tegraVideo(TegraVideo::create(params));

    std::ifstream file;
    if (conf.substituteFramesFilePrefix[0] == '\0')
    {
        if (argc != 2)
        {
            PRINT << "ERROR: File not specified.";
            return 1;
        }
        const char *const filename = argv[1];

        file.open(filename, std::ios::in | std::ios::binary);
        if (!file.is_open())
        {
            PRINT << "Can't open input file " << filename;
            return 1;
        }
    }

    auto buf = new uint8_t[kChunkSize];
    for (;;)
    {
        int dataSize;
        if (conf.substituteFramesFilePrefix[0] == '\0')
        {
            file.read((char*) buf, kChunkSize);
            dataSize = file.gcount();
            if (file.bad())
            {
                PRINT << "ERROR: Unable to read file.";
                return 1;
            }
        }
        else
        {
            static int frameNumber = 0;
            ++frameNumber;
            dataSize = loadFile(
                nx::utils::stringFormat("%s.%d", conf.substituteFramesFilePrefix, frameNumber),
                buf, kChunkSize);
            if (dataSize < 0)
                return 1;
        }

        TegraVideo::CompressedFrame compressedFrame;
        compressedFrame.data = buf;
        compressedFrame.dataSize = dataSize;

        // TODO: TegraVideo: Determine pts.
        compressedFrame.ptsUs = 0;

        if (!tegraVideo->pushCompressedFrame(&compressedFrame))
        {
            PRINT << "ERROR: pushCompressedFrame(dataSize: " << compressedFrame.dataSize
                << ", ptsUs: " << compressedFrame.ptsUs << ") -> false";
            return 1;
        }

        int64_t ptsUs = 0;
        constexpr int kMaxRects = 100;
        TegraVideo::Rect tegraVideoRects[kMaxRects];
        int rectsCount = 0;
        while (tegraVideo->pullRectsForFrame(tegraVideoRects, kMaxRects, &rectsCount, &ptsUs))
        {
            OUTPUT << "pullRectsForFrame() -> {rects.size: " << rectsCount
                << ", ptsUs: " << ptsUs << "}";
        }
        OUTPUT << "pullRectsForFrame() -> false";

        if (file.eof())
        {
            PRINT << "End of video file.";
            return 0;
        }
    }
    delete buf;

    return 0;
}
