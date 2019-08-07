#pragma once

#include <functional>
#include <map>
#include <optional>

#include <QXmlStreamReader>

namespace nx::cloud::aws::api::xml {

namespace detail {

bool advancePastEndElement(QXmlStreamReader* xml);

} // namespace detail

NX_AWS_CLIENT_API bool toBool(const QString& value);

template<typename ObjectType>
struct NX_AWS_CLIENT_API Field
{
    using Assigners = std::map<
        QString/*elementName*/,
        std::function<void(ObjectType* /*outObject*/, const QString& /*value*/)>>;
};

NX_AWS_CLIENT_API std::optional<QString> parseNextElement(QXmlStreamReader* xml);

template<typename ObjectType>
bool parseNextField(
    QXmlStreamReader* xml,
    typename const Field<ObjectType>::Assigners& fieldFuncs,
    ObjectType* outObject)
{
    if (xml->hasError())
        return false;

    auto name = xml->name().toString();

    auto it = fieldFuncs.find(xml->name().toString());
    if (it == fieldFuncs.end())
    {
        xml->readNext();
        return true;
    }

    auto parsedElement = parseNextElement(xml);
    if (!parsedElement)
        return false;

    it->second(outObject, *parsedElement);

    return true;
}

/**
 * Deserializes a simple xml object with no nested structures.
 */
template<typename ObjectType>
bool deserialize(
    QXmlStreamReader* xml,
    typename const Field<ObjectType>::Assigners& assigners,
    const QString& xmlName,
    ObjectType* outObject)
{
    while (!(xml->isEndElement() && xml->name() == xmlName))
    {
        if (!parseNextField(xml, assigners, outObject))
            return false;
    }

    if (!detail::advancePastEndElement(xml))
        return false;

    return true;
}

/**
 * Deserializes a simple xml object with no nested structures into the given vector.
 */
template<typename ObjectType>
bool deserialize(
    QXmlStreamReader* xml,
    typename const xml::Field<ObjectType>::Assigners& assigners,
    const QString& xmlName,
    std::vector<ObjectType>* outVector)
{
    ObjectType object;
    if (!deserialize(xml, assigners, xmlName, &object))
        return false;
    outVector->emplace_back(std::move(object));
    return true;
}

template<typename ObjectType>
bool deserialize(QXmlStreamReader* xml, ObjectType* outObject)
{
}

template<typename ObjectType>
bool deserialize(const QByteArray& data, ObjectType* outObject)
{
    QXmlStreamReader xml(data);
    return deserialize(&xml, outObject);
}

} // namespace nx::cloud::aws::api::xml
