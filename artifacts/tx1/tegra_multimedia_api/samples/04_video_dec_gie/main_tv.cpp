#include "main_tv.h"

#include <iostream>
#include <fstream>
#include <ios>
#include <memory>

#include "tegra_video.h"

#include "config.h"

#define OUTPUT_PREFIX "[video_dec_gie main_tv] "
#include <nx/utils/debug_utils.h>

static constexpr int kChunkSize = 4000000;

int mainTv(int argc, char** argv)
{
    OUTPUT << "mainTv()";

    TegraVideo::Params params;
    params.modelFile = conf.modelFile;
    params.deployFile = conf.deployFile;
    params.cacheFile = conf.cacheFile;
    std::unique_ptr<TegraVideo> tegraVideo(TegraVideo::create(params));

    if (argc != 2)
    {
        std::cerr << "ERROR: File not specified." << std::endl;
        return 1;
    }
    const char *const filename = argv[1];

    std::ifstream in_file;
    in_file.open(filename, std::ios::in | std::ios::binary);
    if (!in_file.is_open())
    {
        std::cerr << "Can't open input file " << filename << std::endl;
        return 1;
    }

    auto buf = new uint8_t[kChunkSize];
    for (;;)
    {
        in_file.read((char*) buf, kChunkSize);
        auto dataSize = in_file.gcount();

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
#if 1
        int64_t ptsUs = 0;
        std::vector<TegraVideo::Rect> tegraVideoRects;
        if (!tegraVideo->pullRectsForFrame(&tegraVideoRects, &ptsUs))
        {
            OUTPUT << "pullRectsForFrame() -> false";
            continue;
        }

        OUTPUT << "pullRectsForFrame() -> {rects.size: " << tegraVideoRects.size()
            << ", ptsUs: " << ptsUs << "}";
#endif // 0
    }
    delete buf;

    return 0;
}
