#pragma once

#include <string>

extern "C" {

#include <gst/gst.h>

} // extern "C"

namespace nx {
namespace gstreamer {

struct Property
{
    std::string name;
    GValue value = G_VALUE_INIT;
};

Property makeBoolProperty(const std::string& propertyName, gboolean propertyValue);

Property makeProperty(const std::string& propertyName, guint propertyValue);

Property makeProperty(const std::string& propertyName, gint propertyValue);

Property makeProperty(const std::string& propertyName, gfloat propertyValue);

Property makeProperty(const std::string& propertyName, gdouble propertyValue);

Property makeProperty(const std::string& propertyName, const char* propertyValue);

Property makeProperty(const std::string& propertyName, GstCaps* propertyValue);

} // namespace gstreamer
} // namespace nx
