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

void connectPads(GstElement* source, GstPad* newPad, gpointer userData);

gboolean busCallback(GstBus* bus, GstMessage* message, gpointer userData);

} // namespace deepstream
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
