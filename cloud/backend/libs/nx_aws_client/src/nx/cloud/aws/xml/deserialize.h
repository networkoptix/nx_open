#pragma once

#include <functional>
#include <map>
#include <optional>

#include <QXmlStreamReader>

namespace nx::cloud::aws::xml {

namespace detail {

bool advancePastEndElement(QXmlStreamReader* reader);

} // namespace detail

bool assign(bool* outField, const QString& value);
bool assign(int* outField, const QString& value);
bool assign(std::string* outField, const QString& value);

template<typename ObjectType>
struct Field
{
    using Parsers = std::map<
        QString/*elementName*/,
        std::function<bool(ObjectType* /*outObject*/, const QString& /*value*/)>>;
};

std::optional<QString> parseNextElement(QXmlStreamReader* reader);

template<typename ObjectType>
bool parseNextField(
    QXmlStreamReader* reader,
    const typename Field<ObjectType>::Parsers& fieldFuncs,
    ObjectType* outObject)
{
    if (reader->hasError())
        return false;

    auto it = fieldFuncs.find(reader->name().toString());
    if (it == fieldFuncs.end())
    {
        reader->readNext();
        return true;
    }

    auto parsedElement = parseNextElement(reader);
    if (!parsedElement)
        return false;

    return it->second(outObject, *parsedElement);
}

/**
 * Deserializes a simple xml object with no nested structures.
 */
template<typename ObjectType>
bool deserialize(
    QXmlStreamReader* reader,
    const typename Field<ObjectType>::Parsers& assigners,
    const QString& xmlName,
    ObjectType* outObject)
{
    while (!(reader->isEndElement() && reader->name() == xmlName))
    {
        if (!parseNextField(reader, assigners, outObject))
            return false;
    }

    if (!detail::advancePastEndElement(reader))
        return false;

    return true;
}

/**
 * Deserializes a simple xml object with no nested structures into the given vector.
 */
template<typename ObjectType>
bool deserialize(
    QXmlStreamReader* reader,
    const typename Field<ObjectType>::Parsers& assigners,
    const QString& xmlName,
    std::vector<ObjectType>* outVector)
{
    ObjectType object;
    if (!deserialize(reader, assigners, xmlName, &object))
        return false;
    outVector->emplace_back(std::move(object));
    return true;
}

template<typename ObjectType>
bool deserialize(QXmlStreamReader* /*reader*/, ObjectType* /*outObject*/)
{
    return false;
}

template<typename ObjectType>
bool deserialize(const QByteArray& data, ObjectType* outObject)
{
    QXmlStreamReader reader(data);
    return deserialize(&reader, outObject);
}

} // namespace nx::cloud::aws::xml
