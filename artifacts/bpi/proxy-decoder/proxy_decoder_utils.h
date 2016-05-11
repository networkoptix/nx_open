#pragma once

#include "flag_config.h"
class ProxyDecoderFlagConfig: public nx::utils::FlagConfig
{
public:
    using nx::utils::FlagConfig::FlagConfig;

    NX_FLAG(0, enableStub);

    // vdpau_helper
    NX_FLAG(0, outputVdpauCalls); //< Log each VDPAU call (errors are logged anyway).
    NX_FLAG(0, enableX11Vdpau); //< Open X11 Display for VDPAU (otherwise, use null for Display).
    NX_FLAG(0, suppressX11LogVdpau); //< If enableX11Vdpau, do not suppress X logging to stderr.

    // proxy_decoder_utils
    NX_FLAG(0, enableOutput);
    NX_FLAG(0, enableTime);

    // proxy_decoder_impl
    NX_FLAG(0, disableGetBits); //< Avoid calling ...get_bits... (thus no picture).
    NX_FLAG(1, enableRgbYOnly); //< Convert only Y to Blue, setting Red and Green to 0.
    NX_FLAG(1, enableRgbPartOnly); //< Convert to RGB only a part of the frame.
    NX_FLAG(0, enableYuvDump); //< Dump frames in both Native and Planar YUV to files.
    NX_FLAG(0, enableLogYuvNative); //< Log struct returned from vdpau with native data ref.
};
extern __attribute__ ((visibility ("hidden"))) ProxyDecoderFlagConfig conf;

// Configuration: should be defined before including this header.
//#define LOG_PREFIX "<ModuleName>: "

#include <cassert>
#include <iostream>
#include <vector>
#include <string>

extern "C" {
#include <libavcodec/vdpau.h>
} // extern "C"

// Log execution of a line - to use, put double-L at the beginning of the line.
#define LL std::cerr << "####### line " << __LINE__ << " [" << __FILE__ << "]\n";

namespace {

struct EndWithEndl
{
    ~EndWithEndl()
    {
        std::cerr << std::endl;
    }
};

} // namespace

#define LOG if (!conf.enableOutput) {} else EndWithEndl() /*operator,*/, std::cerr << LOG_PREFIX

#define PRINT EndWithEndl() /*operator,*/, std::cerr

long getTimeMs();
void logTimeMs(long oldTime, const char* tag);
#define TIME_BEGIN(TAG) long TIME_##TAG = conf.enableTime ? getTimeMs() : 0
#define TIME_END(TAG) if(conf.enableTime) logTimeMs(TIME_##TAG, #TAG)

/**
 * Draw a colored checkerboard in RGB.
 */
void debugDrawCheckerboardArgb(
    uint8_t* argbBuffer, int lineSizeBytes, int frameWidth, int frameHeight);

/**
 * Draw a moving checkerboard in Y channel.
 */
void debugDrawCheckerboardY(
    uint8_t* yBuffer, int lineLen, int frameWidth, int frameHeight);

/**
 * Draw a moving checkerboard in Y channel in Native (32x32-macroblock) format.
 */
void debugDrawCheckerboardYNative(uint8_t* yNative, int frameWidth, int frameHeight);

/**
 * @return Human-readable item index or a "not found" message.
 */
std::string debugDumpRenderStateRef(const vdpau_render_state* renderState,
    const std::vector<vdpau_render_state*>& renderStates);

/**
 * @return Human-readable vdpau_render_state::state.
 */
std::string debugDumpRenderStateFlags(const vdpau_render_state* renderState);
