#pragma once

#include <QXmlStreamWriter>

#include <nx/utils/log/to_string.h>

namespace nx::cloud::aws::xml {

void writeElement(QXmlStreamWriter* writer, const QString& name, const QString& value);
void writeElement(QXmlStreamWriter* writer, const QString& name, const std::string& value);
void writeElement(QXmlStreamWriter* writer, const QString& name, bool value);

template<
    typename NumericType,
    typename = std::enable_if_t<std::is_arithmetic_v<NumericType>, NumericType>>
void writeElement(QXmlStreamWriter* writer, const QString& name, NumericType value)
{
    return writeElement(writer, name, QString::number(value));
}

template<typename ObjectType>
QString className(const ObjectType& object)
{
    QString type = demangleTypeName(typeid(object).name());
    int last = type.lastIndexOf("::");
    return last == -1 ? type : type.mid(last + 2);
}

template<typename ObjectType>
struct NestedObject
{
    NestedObject(QXmlStreamWriter* xml, const ObjectType& object):
        m_xml(xml)
    {
        m_xml->writeStartElement(className(object));
    }

    ~NestedObject()
    {
        m_xml->writeEndElement();
    }
    private:
        QXmlStreamWriter* m_xml;
};

template<typename ObjectType>
void serialize(QXmlStreamWriter* /*xml*/, const ObjectType& /*object*/)
{
}

template<typename ObjectType>
QByteArray serialized(const ObjectType& object)
{
    QByteArray buffer;
    QXmlStreamWriter writer(&buffer);
    writer.writeStartDocument();
    writer.writeStartElement(className(object));

    serialize(&writer, object);

    writer.writeEndElement();
    writer.writeEndDocument();
    return buffer;
}

} // namespace nx::cloud::aws::xml