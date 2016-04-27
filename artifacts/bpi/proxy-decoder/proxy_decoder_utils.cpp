#include "proxy_decoder_utils.h"

#include <chrono>
#include <fstream>

#include "proxy_decoder.h"

ProxyDecoderConf conf("proxydecoder");

bool ENABLE_LOG_VDPAU()
{
    return conf.ENABLE_LOG_VDPAU;
}

bool ENABLE_X11_VDPAU()
{
    return conf.ENABLE_X11_VDPAU;
}

bool SUPPRESS_X11_LOG_VDPAU()
{
    return conf.SUPPRESS_X11_LOG_VDPAU;
}

long getTimeMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void logTimeMs(long oldTimeMs, const char* message)
{
    long timeMs = getTimeMs();
    std::cerr << "TIME ms: " << (timeMs - oldTimeMs) << " [" << message << "]\n";
}

void debugDrawCheckerboardArgb(
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

void debugDrawCheckerboardY(uint8_t* yBuffer, int lineLen, int frameWidth, int frameHeight)
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
            pLine[x0 + x] = (uint8_t) ((((x >> kShift) & 1) == ((line >> kShift) & 1)) ? 0xFE : 0);
        pLine += lineLen;
    }

    if (++x0 >= frameWidth - kBoardSize)
        x0 = 0;
    if (++line0 >= frameHeight - kBoardSize)
        line0 = 0;
}

void debugDrawCheckerboardYNative(uint8_t* yNative, int frameWidth, int frameHeight)
{
    // All coords are in blocks (32x32 pixels).
    static int x0 = 0;
    static int line0 = 0;

    const int w = (frameWidth + 31) / 32;
    const int h = (frameHeight + 31) / 32;

    for (int x = 0; x < 8; ++x)
    {
        for (int y = 0; y < 8; ++y)
        {
            uint8_t color = ((x & 1) == (y & 1)) ? 0 : 254;
            memset(yNative + 32 * 32 * ((line0 + y) * w + (x0 + x)), color, 32 * 32);
        }
    }

    if (++x0 >= (w - /* use only whole blocks */ 1) - 8)
        x0 = 0;
    if (++line0 >= (h - /* use only whole blocks */ 1) - 8)
        line0 = 0;
}

std::string debugDumpRenderStateRef(const vdpau_render_state* renderState,
    const std::vector<vdpau_render_state*>& renderStates)
{
    for (int i = 0; i < renderStates.size(); ++i)
    {
        if (renderStates[i] == renderState)
            return "vdpau_render_state #" + std::to_string(i);
    }
    return "vdpau_render_state NOT_FOUND";
}

std::string debugDumpRenderStateFlags(const vdpau_render_state* renderState)
{
    switch (renderState->state)
    {
        case 0: return "none";
        case FF_VDPAU_STATE_USED_FOR_REFERENCE: return "REFERENCE";
        case FF_VDPAU_STATE_USED_FOR_RENDER: return "RENDER";
        case FF_VDPAU_STATE_USED_FOR_REFERENCE | FF_VDPAU_STATE_USED_FOR_RENDER:
            return "REFERENCE | RENDER";
        default: return std::to_string(renderState->state);
    }
}

//-------------------------------------------------------------------------------------------------

ProxyDecoder* ProxyDecoder::create(int frameWidth, int frameHeight)
{
    conf.reloadSingleFlag(&conf.ENABLE_STUB);
    if (conf.ENABLE_STUB)
        return createStub(frameWidth, frameHeight);
    else
        return createImpl(frameWidth, frameHeight);
}
