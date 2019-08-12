#pragma once

#include <functional>
#include <map>
#include <optional>

#include <QXmlStreamReader>

namespace nx::cloud::aws::xml {

namespace detail {

bool advancePastEndElement(QXmlStreamReader* reader);

} // namespace detail

void assign(bool* var, const QString& value);
void assign(int* var, const QString& value);
void assign(std::string* var, const QString& value);
void assign(std::chrono::system_clock::time_point* var, const QString* value);

template<typename ObjectType>
struct Field
{
    using Assigners = std::map<
        QString/*elementName*/,
        std::function<void(ObjectType* /*outObject*/, const QString& /*value*/)>>;
};

std::optional<QString> parseNextElement(QXmlStreamReader* reader);

template<typename ObjectType>
bool parseNextField(
    QXmlStreamReader* reader,
    const typename Field<ObjectType>::Assigners& fieldFuncs,
    ObjectType* outObject)
{
    if (reader->hasError())
        return false;

    auto name = reader->name().toString();

    auto it = fieldFuncs.find(reader->name().toString());
    if (it == fieldFuncs.end())
    {
        reader->readNext();
        return true;
    }

    auto parsedElement = parseNextElement(reader);
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
    QXmlStreamReader* reader,
    const typename Field<ObjectType>::Assigners& assigners,
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
    const typename Field<ObjectType>::Assigners& assigners,
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
    static_assert(false, "Template specialization is required...");
    return false;
}

template<typename ObjectType>
bool deserialize(const QByteArray& data, ObjectType* outObject)
{
    QXmlStreamReader reader(data);
    return deserialize(&reader, outObject);
}

} // namespace nx::cloud::aws::xml
