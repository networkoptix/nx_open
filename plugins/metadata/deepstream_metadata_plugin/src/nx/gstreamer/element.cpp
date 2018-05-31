#include "element.h"
#include "nx_gstreamer_ini.h"

#define NX_PRINT_PREFIX "nx::gstreamer::Element::"
#include <nx/kit/debug.h>

namespace nx {
namespace gstreamer {

Element::~Element()
{
    if (m_released)
        m_element.release();
}

Element::Element(const FactoryName& factoryName, const ElementName& elementName)
{
    m_element = NativeElementPtr(
        gst_element_factory_make(
            factoryName.c_str(),
            elementName.c_str()),
        [](GstElement* element) { gst_object_unref(element); });
}

Element::Element(Element&& other):
    m_element(std::move(other.m_element)),
    m_released(other.m_released)
{
    other.m_element = nullptr;
    other.m_released = true;
}

bool Element::linkAfter(Element* otherElement)
{
    return gst_element_link(
        GST_ELEMENT(otherElement->nativeElement()),
        GST_ELEMENT(nativeElement())) != FALSE;
}

bool Element::linkBefore(Element* otherElement)
{
    return gst_element_link(
        GST_ELEMENT(nativeElement()),
        GST_ELEMENT(otherElement->nativeElement())) != FALSE;
}

bool Element::linkToRequestPad(
    Element* otherElement,
    const std::string& sourcePadTemplateName,
    const std::string& sinkPadName)
{
    auto sourcePadTemplate = gst_element_class_get_pad_template(
        GST_ELEMENT_GET_CLASS(otherElement->nativeElement()),
        sourcePadTemplateName.c_str());

    auto sourcePad = gst_element_request_pad(
        otherElement->nativeElement(),
        sourcePadTemplate,
        NULL,
        NULL);

    auto sinkPad = gst_element_get_static_pad(
        nativeElement(),
        sinkPadName.c_str());

    const bool result = gst_pad_link(sourcePad, sinkPad)
        == GST_PAD_LINK_OK;

    gst_object_unref(sinkPad);
    gst_object_unref(sourcePad);

    return result;
}

void Element::unlink(Element* otherElement)
{
    gst_element_unlink(m_element.get(), otherElement->nativeElement());
    gst_element_unlink(otherElement->nativeElement(), m_element.get());
}

std::unique_ptr<Pad> Element::pad(const PadName& padName)
{
    return std::make_unique<Pad>(
        gst_element_get_static_pad(
            m_element.get(),
            padName.c_str()));
}

void Element::setProperties(const std::vector<Property>& properties)
{
    for (const auto& property: properties)
    {
        g_object_set_property(
            G_OBJECT(m_element.get()),
            property.name.c_str(),
            &property.value);
    }
}

GstElement*Element::nativeElement() const
{
    return m_element.get();
}

void Element::release()
{
    m_released = true;
}

} // namespace gstreamer
} // namespace nx
