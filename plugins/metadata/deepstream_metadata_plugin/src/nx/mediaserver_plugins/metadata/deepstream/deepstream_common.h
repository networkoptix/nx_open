#pragma once

#include <string>

#include <plugins/plugin_api.h>

extern "C" {

#include <glib.h>
#include <glib-object.h>

} // extern "C"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

static const char* kSinkPadName = "sink";
static const char* kSourcePadName = "src";

// TODO: #dmishin some of these constants should have different values on
// devices different from Jetson.

static const char* kNvidiaElementVideoConverter = "nvvidconv";
static const char* kNvidiaElementGie = "nvcaffegie";
static const char* kNvidiaElementTracker = "nvtracker";
static const char* kNvidiaElementColorDetector = "nvclrdetector";
static const char* kNvidiaElementFaceDetector = "nvfacedetect";

static const char* kGstElementQueue = "queue";
static const char* kGstElementCapsFilter = "capsfilter";
static const char* kGstElementFakeSink = "fakesink";
static const char* kGstElementTee = "tee";
static const char* kGstElementAppSrc = "appsrc";

static const char* kGstElementH264Parser = "h264parse";
static const char* kGstElementH264Decoder = "omxh264dec";
static const char* kGstElementDecodeBin = "decodebin";

static const GQuark kNvidiaIvaMetadataQuark = g_quark_from_static_string("ivameta");

static const nxpl::NX_GUID kCarGuid = {{
    0x5B, 0x0C, 0x6A, 0x41, 0x3A, 0x10, 0x4E, 0x5D,
    0xA0, 0x4B, 0x72, 0x56, 0xA6, 0x24, 0xA6, 0x5D}};

static const nxpl::NX_GUID kHumanGuid = {{
    0x09, 0x3D, 0x30, 0x7A, 0xD0, 0x76, 0x4F, 0xFE,
    0x8D, 0x05, 0xA6, 0x26, 0xF3, 0x6B, 0xF7, 0x0A}};

static const nxpl::NX_GUID kRcGuid = {{
    0x7F, 0xB4, 0xAC, 0x53, 0x7F, 0x3F, 0x47, 0xAD,
    0x87, 0x84, 0x32, 0xBB, 0x91, 0x69, 0xC0, 0x81}};

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
