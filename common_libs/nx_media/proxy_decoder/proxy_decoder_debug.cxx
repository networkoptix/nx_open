// Debug helpers. Intended to be included into "proxydecoder.cpp".

#include <cassert>
#include <iostream>

// Log execution of a line - put double-L at the beginning of the line.
#define LL std::cerr << "####### line " << __LINE__ << "\n";

// Macro ENABLE_LOG should be defined, and LOG_PREFIX will be prepended to each message.
#ifdef ENABLE_LOG
    #define LOG(...) do \
    { \
        std::cerr << LOG_PREFIX << ":" \
            /* Suppress space after ":" if first message char is ":", to get "::". */ \
            << ((#__VA_ARGS__[0] && #__VA_ARGS__[1] == ':') ? "" : " ") \
            << __VA_ARGS__ << "\n"; \
    } while(0)
#else // ENABLE_LOG
    #define LOG(...)
#endif // ENABLE_LOG

namespace {

// ENABLE_TIME macro should be defined to enable time measuring/logging.
#ifdef ENABLE_TIME
    #include <chrono>
    #define TIME_BEGIN(MESSAGE) \
        { std::chrono::milliseconds TIME_t0 = getTime(); const char* const TIME_message = MESSAGE;
    #define TIME_END logTime(TIME_t0, TIME_message); }
#else // ENABLE_TIME
    #define TIME_BEGIN(MESSAGE)
    #define TIME_END
#endif // ENABLE_TIME

#define LOG_YUV_NATIVE(YUV_NATIVE) do \
{ \
    LOG("YuvNative: " \
        << "width: " << (YUV_NATIVE).width \
        << ", height: " << (YUV_NATIVE).height \
        << ", chroma_type: " << (YUV_NATIVE).chroma_type \
        << ", source_format: " << (YUV_NATIVE).source_format \
        << ", luma_size: " << (YUV_NATIVE).luma_size \
        << ", chroma_size: " << (YUV_NATIVE).chroma_size); \
} while (0)

/**
 * Debug: Draw a colored checkerboard in RGB.
 */
static void debugDrawCheckerboardArgb(
    uint8_t* argbBuffer, int lineSizeBytes, int frameWidth, int frameHeight);

/**
 * Draw a moving checkerboard in Y channel.
 */
static void debugDrawCheckerboardY(uint8_t* yBuffer, int lineLen, int frameWidth, int frameHeight);

static void debugDumpToFiles(
    VdpVideoSurface surface,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize);

//-------------------------------------------------------------------------------------------------
// Implementation

#ifdef ENABLE_TIME

static std::chrono::milliseconds getTime()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch());
}

static void logTime(std::chrono::milliseconds oldTime, const char* message)
{
    std::chrono::milliseconds time = getTime();
    std::cerr << "TIME ms: " << (time - oldTime).count() << " [" << message << "]\n";
}

#endif // ENABLE_TIME

static void debugDrawCheckerboardArgb(
    uint8_t* argbBuffer, int lineSizeBytes, int frameWidth, int frameHeight)
{
    static const int kBoardSize = 128;
    static const int kShift = 4; //< log2(kBoardSize / 8)

    static int x0 = 0;
    static int line0 = 0;

    assert((lineSizeBytes & 0x03) == 0);
    const int lineLen = lineSizeBytes >> 2;

    if (!(frameWidth >= kBoardSize && frameHeight >= kBoardSize)) //< Frame is too small.
        return;

    uint32_t* pLine = ((uint32_t*) argbBuffer) + line0 * lineLen;
    for (int line = 0; line < kBoardSize; ++line)
    {
        for (int x = 0; x < kBoardSize; ++x)
            pLine[x0 + x] = (((x >> kShift) & 1) == ((line >> kShift) & 1)) ? 0x006480FE : 0;
        pLine += lineLen;
    }

    if (++x0 >= frameWidth - kBoardSize)
        x0 = 0;
    if (++line0 >= frameHeight - kBoardSize)
        line0 = 0;
}

static void debugDrawCheckerboardY(uint8_t* yBuffer, int lineLen, int frameWidth, int frameHeight)
{
    static const int kBoardSize = 128;
    static const int kShift = 4; //< log2(kBoardSize / 8)

    static int x0 = 0;
    static int line0 = 0;

    if (!(frameWidth >= kBoardSize && frameHeight >= kBoardSize)) //< Frame is too small.
        return;

    uint8_t* pLine = yBuffer + line0 * lineLen;
    for (int line = 0; line < kBoardSize; ++line)
    {
        for (int x = 0; x < kBoardSize; ++x)
            pLine[x0 + x] = (((x >> kShift) & 1) == ((line >> kShift) & 1)) ? 0xFE : 0;
        pLine += lineLen;
    }

    if (++x0 >= frameWidth - kBoardSize)
        x0 = 0;
    if (++line0 >= frameHeight - kBoardSize)
        line0 = 0;
}

FILE* debugCreateFrameDumpFile(int w, int h, int n, const char *suffix, const char *mode)
{
    char filename[100];
    snprintf(filename, sizeof(filename),
        "%s/frame_%dx%d_%d%s", DEBUG_FRAME_PATH, w, h, n, suffix);
    FILE* fp = fopen(filename, mode);
    if (fp == NULL)
    {
        fprintf(stderr, "ERROR: Unable to create file: %s\n", filename);
        assert(false);
    }
    return fp;
}

static void debugDumpFrameToFile(
    void* buffer, int bufferSize, int w, int h, int n, const char* suffix)
{
    FILE* fp = debugCreateFrameDumpFile(w, h, n, suffix, "wb");
    if (fwrite(buffer, 1, bufferSize, fp) != bufferSize)
    {
        fprintf(stderr, "ERROR: Unable to write to a file.\n");
        assert(false);
    }
    fclose(fp);
}

static void debugDumpToFiles(
    VdpVideoSurface surface,
    uint8_t* yBuffer, int yLineSize, uint8_t* uBuffer, uint8_t* vBuffer, int uVLineSize)
{
    static int n = 0;
    ++n;

    YuvNative yuvNative;
    getVideoSurfaceYuvNative(surface, &yuvNative);

    const int w = yuvNative.width;
    const int h = yuvNative.height;
        
    FILE* fp = debugCreateFrameDumpFile(w, h, n, ".txt", "w");
    
    fprintf(fp, "n=%d\n", n);
    fprintf(fp, "yLineSize=%d\n", yLineSize);
    fprintf(fp, "uVLineSize=%d\n", uVLineSize);
    fprintf(fp, "YuvNative.width=%d\n", yuvNative.width);
    fprintf(fp, "YuvNative.height=%d\n", yuvNative.height);

    const char* chromaTypeS = vdpChromaTypeToStr(yuvNative.chroma_type);
    if (chromaTypeS)
        fprintf(fp, "YuvNative.chroma_type=%s\n", chromaTypeS);
    else
        fprintf(fp, "YuvNative.chroma_type=%d\n", yuvNative.chroma_type);
    
    const char *sourceFormatS = vdpYCbCrFormatToStr(yuvNative.source_format);
    if (sourceFormatS)
        fprintf(fp, "YuvNative.source_format=%s\n", sourceFormatS);
    else
        fprintf(fp, "YuvNative.source_format=%d\n", yuvNative.source_format);
    
    fprintf(fp, "YuvNative.luma_size=%d\n", yuvNative.luma_size);
    fprintf(fp, "YuvNative.chroma_size=%d\n", yuvNative.chroma_size);
    
    fclose(fp);

    debugDumpFrameToFile(yBuffer, yLineSize * h, w, h, n, "_y.dat");
    debugDumpFrameToFile(uBuffer, uVLineSize * h / 2, w, h, n, "_u.dat");
    debugDumpFrameToFile(vBuffer, uVLineSize * h / 2, w, h, n, "_v.dat");
    debugDumpFrameToFile(yuvNative.virt, yuvNative.size, w, h, n, "_native.dat");

    fprintf(stderr, "DUMP: Frame %d dumped to %s\n", n, DEBUG_FRAME_PATH);
}

} // namespace
