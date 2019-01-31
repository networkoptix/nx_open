#pragma once

extern "C" {

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

} // extern "C"

namespace nx {
namespace gstreamer {

using PadAddedCallback = void(*)(GstElement* source, GstPad* newPad, gpointer userData);

static const char* kGstreamerDebugDumpDotDirEnv = "GST_DEBUG_DUMP_DOT_DIR";
static const char* kGstreamerDebugLevelEnv = "GST_DEBUG";

static const int kMinGstreamerDebugLevel = 0;
static const int kMaxGstreamerDebugLevel = 8;

} // namespace gstreamer
} // namespace nx
