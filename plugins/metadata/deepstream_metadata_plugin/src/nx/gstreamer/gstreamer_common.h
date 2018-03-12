#pragma once

namespace nx {
namespace gstreamer {

static const char* kGstreamerDebugDumpDotDirEnv = "GST_DEBUG_DUMP_DOT_DIR";
static const char* kGstreamerDebugLevelEnv = "GST_DEBUG";

static const int kMinGstreamerDebugLevel = 0;
static const int kMaxGstreamerDebugLevel = 8;

} // namespace gstreamer
} // namespace nx
