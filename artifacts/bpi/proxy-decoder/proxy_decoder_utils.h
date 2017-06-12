#pragma once

#include <nx/kit/ini_config.h>

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("proxydecoder.ini") {}

    NX_INI_FLAG(0, disable, "Use stub implementation without any display.");
    NX_INI_FLAG(0, enableStub, "Use checkerboard/surface-number stub.");
    NX_INI_FLAG(0, enableStubSurfaceNumbers, "In 'display' mode, draw surface number instead of checkerboard.");
    NX_INI_FLAG(0, enableFpsDisplayDecoded, "Print FPS stat in each displayDecoded().");
    NX_INI_FLAG(0, enableFrameHash, "Compare frame hash to the prev one when printing FPS.");
    NX_INI_FLAG(0, disableCscMatrix, "Avoid setting VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX.");
    NX_INI_INT(8, videoSurfaceCount, "1..16");
    NX_INI_INT(0, outputSurfaceCount, "0 (alloc/dealloc for each frame), 1..255).");

    // vdpau_utils
    NX_INI_FLAG(0, outputVdpauCalls, "Log each VDPAU call (errors are logged anyway).");
    NX_INI_FLAG(0, enableX11Vdpau, "Open X11 Display for VDPAU; otherwise, use null for Display.");
    NX_INI_FLAG(0, suppressX11LogVdpau, "If enableX11Vdpau, do not suppress X logging to stderr.");
    NX_INI_FLAG(0, createX11Window, "Don't use Nx ext to vdpau_sunxi to run without X11 window.");

    // proxy_decoder_utils
    NX_INI_FLAG(0, enableOutput, "");
    NX_INI_FLAG(0, enableTime, "");

    // proxy_decoder_impl
    NX_INI_FLAG(0, disableGetBits, "Avoid calling ...get_bits... (thus no picture).");
    NX_INI_FLAG(1, enableRgbYOnly, "Convert only Y to Blue, setting Red and Green to 0.");
    NX_INI_FLAG(1, enableRgbPartOnly, "Convert to RGB only a part of the frame.");
    NX_INI_FLAG(0, enableYuvDump, "Dump frames in both Native and Planar YUV to files.");
    NX_INI_FLAG(0, enableLogYuvNative, "Log struct returned from vdpau with native data ref.");
};

inline Ini& ini()
{
    static Ini ini;
    return ini;
}

#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <deque>

extern "C" {
#include <libavcodec/vdpau.h>
} // extern "C"

#include <nx/kit/debug.h>

using nx::kit::debug::format;

/**
 * Draw a colored checkerboard in RGB.
 */
void debugDrawCheckerboardArgb(
    uint8_t* argbBuffer, int lineSizeBytes, int frameW, int frameH);

/**
 * Draw a moving checkerboard in Y channel.
 */
void debugDrawCheckerboardY(
    uint8_t* yBuffer, int lineLen, int frameW, int frameH);

/**
 * Draw a moving checkerboard in Y channel in Native (32x32-macroblock) format.
 */
void debugDrawCheckerboardYNative(uint8_t* yNative, int frameW, int frameH);

/**
 * Print the line using a large font with 1 macroblock (32x32 px) as 1 pixel.
 */
void debugPrintNative(uint8_t* yNative, int frameW, int frameH,
    int x0, int y0, const char* text);

/**
 * @return Human-readable item index or a "not found" message.
 */
std::string debugDumpRenderStateRefToStr(const vdpau_render_state* renderState,
    const std::vector<vdpau_render_state*>& renderStates);

/**
 * @return Human-readable vdpau_render_state::state.
 */
std::string debugDumpRenderStateFlagsToStr(const vdpau_render_state* renderState);
