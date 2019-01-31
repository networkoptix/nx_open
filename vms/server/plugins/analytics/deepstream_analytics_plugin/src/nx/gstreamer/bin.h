#pragma once

#include <vector>
#include <map>

#include <nx/gstreamer/element.h>

namespace nx {
namespace gstreamer {

class Bin: public Element
{
    using base_type = Element;

public:
    Bin() = default;
    Bin(const ElementName& binName);
    virtual ~Bin() = default;

    Bin(Bin&& other);
    Bin(const Bin& other) = delete;
    Bin& operator=(Bin& other) = delete;

    bool add(Element* element);
    bool add(const std::vector<Element*>& elements);

    bool link(const std::map<ElementName, ElementName>& nameOfElementsToLink);

    bool createGhostPad(
        Element* element,
        const std::string& elementPadName,
        const std::string& binPadName = std::string());
};

} // namespace gstreamer
} // namespace nx
