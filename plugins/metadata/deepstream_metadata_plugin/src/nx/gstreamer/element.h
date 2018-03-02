#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>

extern "C" {

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>

} // extern "C"

#include <nx/gstreamer/property.h>

namespace nx {
namespace gstreamer {

using FactoryName = std::string;
using ElementName = std::string;

using NativeElementPtr = std::unique_ptr<GstElement, std::function<void(GstElement*)>>;

class Element
{
public:
    Element() = default;
    Element(const FactoryName& factoryName, const ElementName& elementName);
    Element(Element&& other);

    Element(const Element&) = delete;
    Element& operator=(const Element&) = delete;

    virtual ~Element();

    bool linkAfter(Element* otherElement);

    bool linkBefore(Element* otherElement);

    bool linkToRequestPad(
        Element* otherElement,
        const std::string& sourcePadTemplateName,
        const std::string& sinkPadName);

    void unlink(Element* otherElement);

    void setProperties(const std::vector<Property>& properties);

    GstElement* nativeElement() const;

    void release();

protected:
    NativeElementPtr m_element;
    bool m_released = false;
};

} // namespace gstreamer
} // namespace nx
