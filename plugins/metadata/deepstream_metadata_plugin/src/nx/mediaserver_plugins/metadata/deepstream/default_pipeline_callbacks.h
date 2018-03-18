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

void appSourceNeedData(GstElement* appSrc, guint /*unused*/, gpointer userData);

GstPadProbeReturn primaryGieDoneCallback(
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer userData);

GstPadProbeReturn waitForSecondaryGieDoneBufProbe(
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer userData);

GstPadProbeReturn dropOpenAlprFrames(
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer userData);

GstPadProbeReturn processOpenAlprResult(
    GstPad* pad,
    GstPadProbeInfo* info,
    gpointer userData);

gboolean metadataHandlerCallback(GstBuffer* buffer, GstMeta** meta, gpointer userData);

void decodeBinPadAdded(GstElement* source, GstPad* newPad, gpointer userData);

gboolean busCallback(GstBus* bus, GstMessage* message, gpointer userData);

} // namespace deepstream
} // namespace mediaserver_plugins
} // namespace metadata
} // namespace nx
