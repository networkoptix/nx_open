#pragma once

extern "C" {

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

} // extern "C"

namespace nx{
namespace mediaserver_plugins {
namespace metadata {
namespace deepstream {

GstPadProbeReturn waitForSecondaryGieDoneBufProbe(
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer userData);

gboolean handleDefaultMetadata(GstBuffer* buffer, GstMeta** meta, gpointer userData);

} // namespace deepstream
} // namespace mediaserver_plugins
} // namespace metadata
} // namespace nx
