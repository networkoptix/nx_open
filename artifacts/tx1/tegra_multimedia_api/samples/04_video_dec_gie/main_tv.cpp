#include "main_tv.h"

#include <iostream>
#include <fstream>
#include <ios>
#include <memory>
#include <vector>
#include <functional>
#include <atomic>
#include <cstdarg>

#include "tegra_video.h"

#include "video_dec_gie_ini.h"
#include "tegra_video_ini.h"

#define NX_PRINT_PREFIX "[video_dec_gie main_tv] "
#include <nx/kit/debug.h>

namespace {

using nx::kit::debug::format;

static constexpr int kChunkSize = 4000000; //< ATTENTION: Should aссommodate any frame.

#define NX_CRITICAL(COND) \
    [&]() \
    { \
        const auto& cond = (COND); \
        if (!cond) \
        { \
            NX_PRINT << "INTERNAL ERROR: " << __FILE__ << ":" << __LINE__ \
                << ": !(" << #COND << ")"; \
            std::abort(); \
        } \
        return cond; \
    }()

//-------------------------------------------------------------------------------------------------

/** @return EOF if the file is not found, 0 on error. */
static int loadFile(const std::string& filename, uint8_t* buf, int maxSize,
    const std::string& logPrefix)
{
    std::ifstream file;
    file.open(filename, std::ios::in | std::ios::binary);
    if (!file.is_open())
    {
        NX_OUTPUT << logPrefix << "Unable to open file: " << filename;
        return EOF;
    }

    int dataSize;

    file.read((char*) buf, maxSize);
    if (file.bad())
    {
        NX_PRINT << logPrefix << "ERROR: Unable to read file: " << filename;
        return 0;
    }

    dataSize = (int) file.gcount();
    if (dataSize <= 0)
    {
        NX_PRINT << logPrefix << "ERROR: file.read() <= 0 for file: " << filename;
        return 0;
    }

    if (!file.eof())
    {
        NX_PRINT << logPrefix << "ERROR: Frame file [" << filename << "] is larger than the buffer ("
            << maxSize << " bytes)";
        return 0;
    }

    NX_OUTPUT << logPrefix << "Substituted frame from file: " << filename;
    return dataSize;
}

//-------------------------------------------------------------------------------------------------

class TegraVideoExecutor
{
public:
    /**
     * Process compressed video either from a video file, or from pre-generated frame files.
     * @param videoFilename Video file to be read in chunks, unless frameFilePrefix is non-empty.
     * @param frameFilePrefix If not empty, each frame is loaded from file "frameFilePrefix.#".
     */
    TegraVideoExecutor(
        const std::string& id,
        const char* videoFilename,
        const char* frameFilePrefix,
        int bufSize,
        const TegraVideo::Params& params)
        :
        m_id(id),
        m_videoFilename(NX_CRITICAL(videoFilename)),
        m_frameFilePrefix(NX_CRITICAL(frameFilePrefix)),
        m_bufSize(bufSize),
        m_buf(new std::vector<uint8_t>(bufSize))
    {
        auto paramsWithId = params;
        paramsWithId.id = id.c_str();
        m_tegraVideo.reset(tegraVideoCreate());
        m_tegraVideo->start(&paramsWithId);
    }

    bool execute()
    {
        if (m_frameFilePrefix.empty())
        {
            NX_CRITICAL(m_frameFilePrefix.empty());

            m_videoFile.open(m_videoFilename, std::ios::in | std::ios::binary);
            if (!m_videoFile.is_open())
            {
                NX_PRINT << "Executor #" << m_id << ": Can't open video file " << m_videoFilename;
                return false;
            }
        }

        for (;;)
        {
            const int dataSize = fillNextBuf();
            if (dataSize == EOF)
            {
                NX_PRINT << "Executor #" << m_id << ": End of the video.";
                return true;
            }
            if (dataSize <= 0)
                return false;

            if (!processBuf(dataSize))
                return false;
        }
    }

private:
    /** @return Number of bytes read, EOF if there is no more data, 0 on error. */
    int fillNextBuf()
    {
        int dataSize;
        if (m_frameFilePrefix.empty()) // Reading from the video file.
        {
            if (m_videoFile.eof())
                return EOF;

            m_videoFile.read((char*) m_buf->data(), m_bufSize);

            if (m_videoFile.bad())
            {
                NX_PRINT << "Executor #" << m_id << ": ERROR: Unable to read file.";
                return 0;
            }

            dataSize = (int) m_videoFile.gcount();
            if (dataSize == 0)
                return EOF;
        }
        else // Reading from a frame file.
        {
            ++m_frameFileNumber;
            dataSize = loadFile(
                format("%s.%d", m_frameFilePrefix.c_str(), m_frameFileNumber),
                m_buf->data(), m_bufSize, format("Executor #%d: ", m_id));
        }
        return dataSize;
    }

    bool processBuf(int dataSize)
    {
        // NOTE: Current impl of TegraVideo allows arbitrary consecutive chunks of a video file
        // in struct CompressedFrame - it does not have to be exactly one frame.
        TegraVideo::CompressedFrame compressedFrame;
        compressedFrame.data = m_buf->data();
        compressedFrame.dataSize = dataSize;

        // TODO: TegraVideo: Determine pts.
        compressedFrame.ptsUs = 0;

        if (!m_tegraVideo->pushCompressedFrame(&compressedFrame))
        {
            NX_PRINT << "Executor #" << m_id << ": ERROR: pushCompressedFrame(dataSize: "
                << compressedFrame.dataSize << ", ptsUs: " << compressedFrame.ptsUs
                << ") -> false";
            return false;
        }

        int64_t ptsUs = 0;
        constexpr int kMaxRects = 100;
        TegraVideo::Rect tegraVideoRects[kMaxRects];
        int rectsCount = 0;
        while (m_tegraVideo->pullRectsForFrame(
            tegraVideoRects, kMaxRects, &rectsCount, &ptsUs))
        {
            NX_OUTPUT << "Executor #" << m_id << ": pullRectsForFrame() -> {rects.size: "
                << rectsCount << ", ptsUs: " << ptsUs << "}";
        }
        NX_OUTPUT << "Executor #" << m_id << ": pullRectsForFrame() -> false";
        return true;
    }

private:
    const std::string m_id;
    const std::string m_frameFilePrefix;
    const std::string m_videoFilename;
    std::unique_ptr<std::vector<uint8_t>> m_buf;
    const int m_bufSize;
    std::ifstream m_videoFile;
    std::unique_ptr<TegraVideo, decltype(&tegraVideoDestroy)> m_tegraVideo{
        nullptr, tegraVideoDestroy};
    int m_frameFileNumber = 0;
};

//-------------------------------------------------------------------------------------------------

static void* executorThread(void* arg)
{
    TegraVideoExecutor* executor = (TegraVideoExecutor*) arg;
    if (!executor->execute())
        return (void*) "ERROR";
    else
        return nullptr;
}

} // namespace

int mainTv(int argc, char** argv)
{
    NX_OUTPUT << "mainTv()";

    TegraVideo::Params params;
    params.modelFile = exeIni().modelFile;
    params.deployFile = exeIni().deployFile;
    params.cacheFile = exeIni().cacheFile;
    params.netWidth = exeIni().netWidth;
    params.netHeight = exeIni().netHeight;

    const char* videoFilename = "";
    if (exeIni().substituteFramesFilePrefix[0] == '\0')
    {
        if (argc != 2)
        {
            NX_PRINT << "ERROR: File not specified.";
            return 1;
        }
        videoFilename = argv[1];
    }

    if (exeIni().tegraVideoCount < 1)
    {
        NX_PRINT << "ERROR: .ini tegraVideoCount should be 1 or more.";
        return 1;
    }

    std::vector<std::unique_ptr<TegraVideoExecutor>> executors;
    std::vector<pthread_t> threads;
    for (int i = 0; i < exeIni().tegraVideoCount; ++i)
    {
        executors.emplace_back(new TegraVideoExecutor(
            format("%d", i),
            videoFilename,
            exeIni().substituteFramesFilePrefix,
            kChunkSize,
            params));

        threads.emplace_back(0);

        pthread_create(&threads.back(), NULL, executorThread, executors.back().get());
    }

    bool allSucceeded = true;
    for (int i = 0; i < threads.size(); ++i)
    {
        const auto& thread = threads[i];
        void* result;
        if (const auto errorCode = pthread_join(thread, &result))
        {
            NX_PRINT << "ERROR: pthread_join() failed for thread #" << thread << ".";
        }
        else
        {
            if (result != nullptr)
                allSucceeded = false;
        }

    }

    return allSucceeded ? 0 : 1;
}
