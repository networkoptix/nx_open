#include "proxy_decoder_utils.h"

#include <fstream>

#include "proxy_decoder.h"

void debugDrawCheckerboardArgb(
    uint8_t* argbBuffer, int lineSizeBytes, int frameW, int frameH)
{
    static const int kBoardSize = 128;
    static const int kShift = 4; //< log2(kBoardSize / 8)

    static int x0 = 0;
    static int line0 = 0;

    assert((lineSizeBytes & 0x03) == 0);
    const int lineLen = lineSizeBytes >> 2;

    if (!(frameW >= kBoardSize && frameH >= kBoardSize)) //< Frame is too small.
        return;

    uint32_t* pLine = ((uint32_t*) argbBuffer) + line0 * lineLen;
    for (int line = 0; line < kBoardSize; ++line)
    {
        for (int x = 0; x < kBoardSize; ++x)
            pLine[x0 + x] = (((x >> kShift) & 1) == ((line >> kShift) & 1)) ? 0x006480FE : 0;
        pLine += lineLen;
    }

    if (++x0 >= frameW - kBoardSize)
        x0 = 0;
    if (++line0 >= frameH - kBoardSize)
        line0 = 0;
}

void debugDrawCheckerboardY(uint8_t* yBuffer, int lineLen, int frameW, int frameH)
{
    static const int kBoardSize = 128;
    static const int kShift = 4; //< log2(kBoardSize / 8)

    static int x0 = 0;
    static int line0 = 0;

    if (!(frameW >= kBoardSize && frameH >= kBoardSize)) //< Frame is too small.
        return;

    uint8_t* pLine = yBuffer + line0 * lineLen;
    for (int line = 0; line < kBoardSize; ++line)
    {
        for (int x = 0; x < kBoardSize; ++x)
            pLine[x0 + x] = (uint8_t) ((((x >> kShift) & 1) == ((line >> kShift) & 1)) ? 0xFE : 0);
        pLine += lineLen;
    }

    if (++x0 >= frameW - kBoardSize)
        x0 = 0;
    if (++line0 >= frameH - kBoardSize)
        line0 = 0;
}

namespace {

static void debugPutPixelNative(uint8_t* yNative, int frameW, int frameH,
    int x, int y, int color)
{
    const int w = (frameW + 31) / 32;
    const int h = (frameH + 31) / 32;

    if (x >= 0 && x < w && y >= 0 && y < h)
        memset(yNative + 32 * 32 * (y * w + x), color, 32 * 32);
}

static void debugPutPixelNative(uint8_t* yNative, int frameW, int frameH,
    int x, int y, bool color)
{
    debugPutPixelNative(yNative, frameW, frameH, x, y, color ? 0xFE : 0);
}

} // namespace

void debugDrawCheckerboardYNative(uint8_t* yNative, int frameW, int frameH)
{
    // All coords are in blocks (32x32 pixels).
    static int x0 = 0;
    static int line0 = 0;
    static const int kBoardWidth = 8;
    static const int kBoardHeight = 8;

    const int w = (frameW + 31) / 32;
    const int h = (frameH + 31) / 32;

    for (int x = 0; x < kBoardWidth; ++x)
    {
        for (int y = 0; y < kBoardHeight; ++y)
        {
            debugPutPixelNative(yNative, frameW, frameH, x0 + x, line0 + y, (x & 1) == (y & 1));
        }
    }

    if (++x0 >= (w - /* use only whole blocks */ 1) - kBoardWidth)
        x0 = 0;
    if (++line0 >= (h - /* use only whole blocks */ 1) - kBoardHeight)
        line0 = 0;
}

namespace {

static const int kFontWidth = 4;
static const int kFontHeight = 6;
static const int kFontStride = 32;
static const char kFont[][kFontHeight * kFontWidth * kFontStride + /*'\0'*/ 1] =
{//   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   /   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ?
"     0  0 0 0 0  0  0 0  0   0    0 0   0 0  0                0 000  0  000 000 0 0 000 000 000 000 000           0     0   000 "
"     0  0 0 000 000   0 0 0  0   0   0   0   0                0 0 0 00    0   0 0 0 0   0     0 0 0 0 0  0   0   0  000  0    0 "
"     0      0 0 00   0   0       0   0  000 000     000      0  0 0  0  000 000 000 000 000   0 000 000         0         0  0  "
"            000  00 0   0 0      0   0   0   0              0   0 0  0  0     0   0   0 0 0   0 0 0   0  0       0  000  0      "
"     0      0 0 000 0 0  00       0 0   0 0  0   0       0  0   000 000 000 000   0 000 000   0 000 000      0    0     0    0  "
"                 0                              0                                                           0                   "
,//@  A   B   C   D   E   F   G   H   I   J   K   L   M   N   O   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _
"000  0  00   00 00  000 000  00 0 0 000 000 0 0 0   0 0 00   0  00   0  00   00 000 0 0 0 0 0 0 0 0 0 0 000  00 0   00   0      "
"  0 0 0 0 0 0   0 0 0   0   0   0 0  0    0 0 0 0   000 0 0 0 0 0 0 0 0 0 0 0    0  0 0 0 0 0 0 0 0 0 0   0  0  0    0  0 0     "
"000 000 00  0   0 0 000 000 0   000  0    0 00  0   000 0 0 0 0 00  0 0 00   0   0  0 0 0 0 000  0   0   0   0   0   0          "
"000 0 0 0 0 0   0 0 0   0   0 0 0 0  0    0 0 0 0   0 0 0 0 0 0 0   000 0 0   0  0  0 0  0  000 0 0  0  0    0    0  0          "
"000 0 0 00   00 00  000 0    00 0 0 000 00  0 0 000 0 0 0 0  0  0    00 0 0 00   0   0   0  0 0 0 0  0  000  00   0 00      000 "
"                                                                                                                                "
,//`  a   b   c   d   e   f   g   h   i   j   k   l   m   n   o   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~
"0       0         0       0     0    0   0  0   00                               0                           00  0  00   0 0    "
" 0   00 0    00   0  0   0   0  0           0    0  00  00   0  00   00  00  00  0  0 0 0 0 0 0 0 0 0 0 000  0   0   0  0 0     "
"      0 00  0    00 0 0 000 0 0 00  00  00  0 0  0  000 0 0 0 0 0 0 0 0 0   00  000 0 0 0 0 000  0   0   00 0    0    0         "
"    000 0 0 0   0 0 00   0   00 0 0  0   0  00   0  000 0 0 0 0 0 0 0 0 0     0  0  0 0  0  000  0   0  0    0   0   0          "
"    000 00   00  00  00  0    0 0 0 000  0  0 0 000 0 0 0 0  0  00   00 0   00    0  00  0  0 0 0 0  0  000  00  0  00          "
"                            00          0                       0     0                                                         "
};

} // namespace

void debugPrintNative(uint8_t* yNative, int frameW, int frameH,
    int x0, int y0, const char* text)
{
    const int len = strlen(text);
    for (int c = 0; c < len; ++c)
    {
        const int fontLine = (text[c] - ' ') / kFontStride;
        const char* start;
        if (fontLine < 0 || fontLine >= sizeof(kFont) / sizeof(kFont[0])) //< Char is not in font.
            start = &kFont[0][kFontWidth * (' ' % kFontStride)]; //< Use ' ' as a substitute char.
        else
            start = &kFont[fontLine][kFontWidth * (text[c] % 32)];

        for (int y = y0; y < y0 + kFontHeight; ++y)
        {
            for (int x = x0; x < x0 + kFontWidth; ++x)
            {
                debugPutPixelNative(yNative, frameW, frameH,
                    kFontWidth * c + x, y,
                    start[kFontWidth * kFontStride * (y - y0) + x - x0] != ' ');
            }
        }
    }
}

std::string debugDumpRenderStateRefToStr(const vdpau_render_state* renderState,
    const std::vector<vdpau_render_state*>& renderStates)
{
    for (int i = 0; i < renderStates.size(); ++i)
    {
        if (renderStates[i] == renderState)
        {
            return format("vdpau_render_state %02d of %d {videoSurface handle #%02d}",
                i, renderStates.size(), renderState->surface);
        }
    }
    return "vdpau_render_state NOT_FOUND";
}

std::string debugDumpRenderStateFlagsToStr(const vdpau_render_state* renderState)
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
