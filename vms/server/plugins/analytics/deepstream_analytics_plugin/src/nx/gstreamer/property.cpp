#include "property.h"
#include "nx_gstreamer_ini.h"

#define NX_PRINT_PREFIX "deepstream::makeProperty "
#include <nx/kit/debug.h>

namespace nx {
namespace gstreamer {

Property makeBoolProperty(const std::string& propertyName, gboolean propertyValue)
{
    NX_OUTPUT << __func__ << " Making bool property: " << propertyName << " " << propertyValue;
    Property property;
    property.name = propertyName;
    g_value_init(&property.value, G_TYPE_BOOLEAN);
    g_value_set_boolean(&property.value, propertyValue);
    return property;
}

Property makeProperty(const std::string& propertyName, guint propertyValue)
{
    NX_OUTPUT << __func__ << " Making uint property: " << propertyName << " " << propertyValue;
    Property property;
    property.name = propertyName;
    g_value_init(&property.value, G_TYPE_UINT);
    g_value_set_uint(&property.value, propertyValue);
    return property;
}

Property makeProperty(const std::string& propertyName, gint propertyValue)
{
    NX_OUTPUT << __func__ << " Making int property: " << propertyName << " " << propertyValue;
    Property property;
    property.name = propertyName;
    g_value_init(&property.value, G_TYPE_INT);
    g_value_set_int(&property.value, propertyValue);
    return property;
}

Property makeProperty(const std::string& propertyName, gfloat propertyValue)
{
    NX_OUTPUT << __func__ << " Making float property: " << propertyName << " " << propertyValue;
    Property property;
    property.name = propertyName;
    g_value_init(&property.value, G_TYPE_FLOAT);
    g_value_set_float(&property.value, propertyValue);
    return property;
}

Property makeProperty(const std::string& propertyName, gdouble propertyValue)
{
    NX_OUTPUT << __func__ << " Making double property: " << propertyName << " " << propertyValue;
    Property property;
    property.name = propertyName;
    g_value_init(&property.value, G_TYPE_DOUBLE);
    g_value_set_double(&property.value, propertyValue);
    return property;
}

Property makeProperty(const std::string& propertyName, const char* propertyValue)
{
    NX_OUTPUT << __func__ << " Making string property: " << propertyName << " " << propertyValue;
    Property property;
    property.name = propertyName;
    g_value_init(&property.value, G_TYPE_STRING);
    g_value_set_string(&property.value, propertyValue);
    return property;
}

Property makeProperty(const std::string& propertyName, GstCaps* propertyValue)
{
    // TODO: #dmishin - This method doesn't work, fix it.
    NX_OUTPUT << __func__ << " Making GstCaps property: " << propertyName << " " << propertyValue;
    Property property;
    property.name = propertyName;
    g_value_init_from_instance(&property.value, propertyValue);
    g_value_set_instance(&property.value, propertyValue);
    return property;
}

} // namespace gstreamer
} // namespace nx
