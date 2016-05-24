#pragma once

#include "flag_config.h"
struct NX_UTILS_API ProxyDecoderFlagConfig: public nx::utils::FlagConfig
{
    using nx::utils::FlagConfig::FlagConfig;

    NX_FLAG(0, disable, "Use stub implementation without any display.");
    NX_FLAG(0, enableStub, "Use checkerboard/surface-number stub.");
    NX_FLAG(0, enableStubSurfaceNumbers, "In 'display' mode, draw surface number instead of checkerboard.");
    NX_FLAG(0, enableFps, "");
    NX_FLAG(0, disableCscMatrix, "Avoid setting VDP_VIDEO_MIXER_ATTRIBUTE_CSC_MATRIX.");
    NX_INT_PARAM(8, videoSurfaceCount, "1..16");
    NX_INT_PARAM(0, outputSurfaceCount, "0 (alloc/dealloc for each frame), 1..255).");

    // vdpau_helper
    NX_FLAG(0, outputVdpauCalls, "Log each VDPAU call (errors are logged anyway).");
    NX_FLAG(0, enableX11Vdpau, "Open X11 Display for VDPAU (otherwise, use null for Display).");
    NX_FLAG(0, suppressX11LogVdpau, "If enableX11Vdpau, do not suppress X logging to stderr.");
    NX_FLAG(0, createX11Window, "Note: Nx ext to vdpau_sunxi allows to run with X11 without a window.");

    // proxy_decoder_utils
    NX_FLAG(0, enableOutput, "");
    NX_FLAG(0, enableTime, "");

    // proxy_decoder_impl
    NX_FLAG(0, disableGetBits, "Avoid calling ...get_bits... (thus no picture).");
    NX_FLAG(1, enableRgbYOnly, "Convert only Y to Blue, setting Red and Green to 0.");
    NX_FLAG(1, enableRgbPartOnly, "Convert to RGB only a part of the frame.");
    NX_FLAG(0, enableYuvDump, "Dump frames in both Native and Planar YUV to files.");
    NX_FLAG(0, enableLogYuvNative, "Log struct returned from vdpau with native data ref.");
};
extern NX_UTILS_API ProxyDecoderFlagConfig conf;

// Configuration: should be defined before including this header.
//#define OUTPUT_PREFIX "<ModuleName>: "

#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <memory>

extern "C" {
#include <libavcodec/vdpau.h>
} // extern "C"

// Log execution of a line - to use, put double-L at the beginning of the line.
#define LL std::cerr << "####### line " << __LINE__ << " [" << __FILE__ << "]\n";

namespace {

template<typename... Args>
std::string stringFormat(const std::string& format, Args... args)
{
    size_t size = snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'.
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside.
}

struct EndWithEndl
{
    ~EndWithEndl()
    {
        std::cerr << std::endl;
    }
};

} // namespace

#define OUTPUT if (!conf.enableOutput) {} else EndWithEndl() /*operator,*/, std::cerr << OUTPUT_PREFIX

#define PRINT EndWithEndl() /*operator,*/, std::cerr << OUTPUT_PREFIX

long getTimeMs();
void logTimeMs(long oldTime, const char* tag);
#define TIME_BEGIN(TAG) long TIME_##TAG = conf.enableTime ? getTimeMs() : 0
#define TIME_END(TAG) if(conf.enableTime) logTimeMs(TIME_##TAG, #TAG)

void debugShowFps(const char* prefix);

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
 * Currently only numbers and spaces are supported.
 */
void debugPrintNative(uint8_t* yNative, int frameW, int frameH,
    int x0, int y0, const char* text);

/**
 * @return Human-readable item index or a "not found" message.
 */
std::string debugDumpRenderStateRef(const vdpau_render_state* renderState,
    const std::vector<vdpau_render_state*>& renderStates);

/**
 * @return Human-readable vdpau_render_state::state.
 */
std::string debugDumpRenderStateFlags(const vdpau_render_state* renderState);
