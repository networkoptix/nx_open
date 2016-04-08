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

/**
 * Debug: Draw a colored checkerboard in RGB.
 */
static void debugDrawCheckerboardArgb(
    uint8_t* argbBuffer, int lineSizeBytes, int frameWidth, int frameHeight);

/**
 * Draw a moving checkerboard in Y channel.
 */
static void debugDrawCheckerboardY(uint8_t* yBuffer, int lineLen, int frameWidth, int frameHeight);

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

} // namespace
