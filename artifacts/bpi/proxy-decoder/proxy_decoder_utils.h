#pragma once

#include "conf.h"

class ProxyDecoderConf: public Conf
{
public:
    using Conf::Conf;

    FLAG(0, ENABLE_STUB);

    // vdpau_helper
    FLAG(0, ENABLE_LOG_VDPAU); //< Log each VDPAU call (errors are logged anyway).
    FLAG(0, ENABLE_X11_VDPAU); //< Open X11 Display for VDPAU (otherwise, supply null for Display).
    FLAG(0, SUPPRESS_X11_LOG_VDPAU); //< If ENABLE_X11_VDPAU, do not suppress X logging to stderr.

    // proxy_decoder_utils
    FLAG(0, ENABLE_LOG);
    FLAG(0, ENABLE_TIME);

    // proxy_decoder_impl
    FLAG(0, DISABLE_GET_BITS); //< Avoid calling ...get_bits... (thus no picture).
    FLAG(1, ENABLE_RGB_Y_ONLY); //< Convert only Y to Blue, setting Red and Green to 0.
    FLAG(1, ENABLE_RGB_PART_ONLY); //< Convert to RGB only a part of the frame.
    FLAG(0, ENABLE_YUV_DUMP); //< Dump frames in both Native and Planar YUV to files.
    FLAG(0, ENABLE_LOG_YUV_NATIVE); //< Log struct returned from vdpau with native data ref.
};
extern ProxyDecoderConf conf;

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

#define LOG if (!conf.ENABLE_LOG) {} else EndWithEndl() /*operator,*/, std::cerr << LOG_PREFIX

#define PRINT EndWithEndl() /*operator,*/, std::cerr

long getTimeMs();

void logTimeMs(long oldTime, const char* message);

#define TIME_BEGIN(TAG) long TIME_##TAG = conf.ENABLE_TIME ? getTimeMs() : 0
#define TIME_END(TAG) if(conf.ENABLE_TIME) logTimeMs(TIME_##TAG, #TAG)

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
 * @return Human-readable item index or a "not found" message.
 */
std::string debugDumpRenderStateRef(const vdpau_render_state* renderState,
    const std::vector<vdpau_render_state*>& renderStates);

/**
 * @return Human-readable vdpau_render_state::state.
 */
std::string debugDumpRenderStateFlags(const vdpau_render_state* renderState);
