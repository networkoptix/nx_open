#pragma once

extern "C" {

#include <gst/gst.h>

} // extern "C"

namespace nx{
namespace vms_server_plugins {
namespace analytics {
namespace deepstream {

void appSourceNeedData(GstElement* appSrc, guint /*unused*/, gpointer userData);

void connectPads(GstElement* source, GstPad* newPad, gpointer userData);

gboolean busCallback(GstBus* bus, GstMessage* message, gpointer userData);

} // namespace deepstream
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
