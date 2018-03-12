#include "bin.h"
#include "scope_guard.h"

namespace nx {
namespace gstreamer {

Bin::Bin(const ElementName& binName)
{
    m_element = NativeElementPtr(
        GST_ELEMENT(gst_bin_new(binName.c_str())),
                [](GstElement* element) { gst_object_unref(element); });
}

Bin::Bin(Bin&& other):
    base_type(std::move(other))
{
}

bool Bin::add(Element* element)
{
    const auto result = gst_bin_add(
        GST_BIN(m_element.get()),
        GST_ELEMENT(element->nativeElement()));

    if (result != FALSE)
        element->release();

    return result != FALSE;
}

bool Bin::add(const std::vector<Element*>& elements)
{
    bool result = false;
    auto guard(makeScopeGuard(
        [&elements]()
        {
            for (auto& element: elements)
                element->release();
        }));

    for (auto& element: elements)
    {
        result = gst_bin_add(
            GST_BIN(m_element.get()),
            GST_ELEMENT(element->nativeElement()));

        if (result == FALSE)
            return false;
    }

    return true;
}

bool Bin::link(const std::map<ElementName, ElementName>& nameOfElementsToLink)
{
    for (const auto& elementNamesPair: nameOfElementsToLink)
    {
        std::pair<GstElement*, GstElement*> elementsToLink;
        auto scopeGuard(makeScopeGuard(
            [&elementsToLink]()
            {
                if (elementsToLink.first)
                    gst_object_unref(elementsToLink.first);

                if (elementsToLink.second)
                    gst_object_unref(elementsToLink.second);
            }));

        elementsToLink.first = gst_bin_get_by_name(
            GST_BIN(m_element.get()),
            elementNamesPair.first.c_str());

        elementsToLink.second = gst_bin_get_by_name(
            GST_BIN(m_element.get()),
            elementNamesPair.second.c_str());

        if (!elementsToLink.first || !elementsToLink.second)
            return false;

        const auto linkResult = gst_element_link(
            GST_ELEMENT(elementsToLink.first),
            GST_ELEMENT(elementsToLink.second));

        if (linkResult == FALSE)
            return false;
    }

    return true;
}

bool Bin::createGhostPad(
    Element* element,
    const std::string& elementPadName,
    const std::string& binPadName)
{
    auto pad = gst_element_get_static_pad(
        element->nativeElement(),
        elementPadName.c_str());

    auto binGhostPad = binPadName.empty()
        ? gst_ghost_pad_new(elementPadName.c_str(), pad)
        : gst_ghost_pad_new(binPadName.c_str(), pad);

    gst_element_add_pad(nativeElement(), binGhostPad);
    gst_object_unref(G_OBJECT(pad));

    return true;
}

} // namespace gstreamer
} // namespace nx
