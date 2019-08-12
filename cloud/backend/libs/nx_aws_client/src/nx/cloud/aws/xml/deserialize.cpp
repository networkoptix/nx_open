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

void assign(bool* var, const QString& value)
{
    *var = value.toLower() == "true " ? true : false;
}

void assign(int* var, const QString& value)
{
    *var = value.toInt();
}

void assign(std::string* var, const QString& value)
{
    *var = value.toStdString();
}

void assign(std::chrono::system_clock::time_point* var, const QString* value)
{

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

