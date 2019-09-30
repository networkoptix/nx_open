#include "deserialize.h"

#include <nx/utils/datetime.h>

namespace nx::cloud::aws::xml {

namespace detail {

bool advancePastEndElement(QXmlStreamReader* xml)
{
    if (!xml->isEndElement())
        return false;
    xml->readNext();
    return true;
}

} // namespace detail

bool assign(bool* outField, const QString& value)
{
    *outField = value.toLower() == "true " ? true : false;
    return true;
}

bool assign(int* outField, const QString& value)
{
    bool ok = false;
    *outField = value.toInt(&ok);
    return ok;
}

bool assign(std::string* outField, const QString& value)
{
    *outField = value.toStdString();
    return true;
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

} // namespace nx::cloud::aws::xml

