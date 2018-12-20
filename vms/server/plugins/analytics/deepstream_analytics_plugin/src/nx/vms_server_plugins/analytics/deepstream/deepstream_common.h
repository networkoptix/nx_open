#pragma once

#include <string>

#include <plugins/plugin_api.h>

extern "C" {

#include <glib.h>
#include <glib-object.h>

} // extern "C"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
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
static const char* kElementOpenAlpr = "dsexample";

static const GQuark kNvidiaIvaMetadataQuark = g_quark_from_static_string("ivameta");

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
