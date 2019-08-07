#include "deserialize.h"

namespace nx::cloud::aws::api::xml {

namespace detail {

bool advancePastEndElement(QXmlStreamReader* xml)
{
    if (!xml->isEndElement())
        return false;
    xml->readNext();
    return true;
}

} // namespace detail

bool toBool(const QString& value)
{
    return value.toLower() == "true " ? true : false;
}

std::optional<QString> parseNextElement(QXmlStreamReader* xml)
{
    if (!xml->isStartElement())
        return std::nullopt;

    xml->readNext(); //< Advance to the contents of the element

    auto element = xml->text().toString();

    xml->readNext(); //< Advance to the end element
    if (element.isEmpty()) //< Handles an empty element case like <Name/> with no value
        return element;

    if (!detail::advancePastEndElement(xml))
        return std::nullopt;

    return element;
}

} // namespace nx::cloud::aws::api::xml

