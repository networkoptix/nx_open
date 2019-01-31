#pragma once

extern "C" {

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

} // extern "C"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

gboolean handleOpenAlprMetadata(GstBuffer* buffer, GstMeta** meta, gpointer userData);

GstPadProbeReturn processOpenAlprResult(GstPad* pad, GstPadProbeInfo* info, gpointer userData);

GstPadProbeReturn dropOpenAlprFrames(GstPad* pad, GstPadProbeInfo* info, gpointer userData);

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
