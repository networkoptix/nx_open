#include "xml_parse_helper.h"

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
    if (!xml->isCharacters())
        return std::nullopt;

    auto element = xml->text().toString();

    xml->readNext(); //< Advance to the end element
    if (!detail::advancePastEndElement(xml))
        return std::nullopt;

    return element;
}

} // namespace nx::cloud::aws::api::xml

