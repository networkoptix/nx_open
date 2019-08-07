#pragma once

#include <QXmlStreamWriter>

namespace nx::cloud::aws::api::xml {

void writeElement(QXmlStreamWriter* xml, const QString& name, const QString& value);
void writeElement(QXmlStreamWriter* xml, const QString& name, const std::string& value);
void writeElement(QXmlStreamWriter* xml, const QString& name, bool value);
void writeElement(QXmlStreamWriter* xml, const QString& name, int value);

template<typename ObjectType>
QString className(const ObjectType& object)
{
    QString type = typeid(object).name();
    int last = type.lastIndexOf("::");
    return last == -1 ? type : type.mid(last + 2);
}

template<typename ObjectType>
struct ScopedElement
{
    QXmlStreamWriter* xml;
    QString element;

    ScopedElement(QXmlStreamWriter* xml, const ObjectType& object):
        xml(xml),
        element(className(object))
    {
        xml->writeStartElement(element);
    }

    ~ScopedElement()
    {
        xml->writeEndElement();
    }
};

template<typename ObjectType>
void serialize(QXmlStreamWriter* /*xml*/, const ObjectType& /*object*/)
{
}

template<typename ObjectType>
QByteArray serialized(const ObjectType& object)
{
    QByteArray buffer;
    QXmlStreamWriter xml(&buffer);
    xml.writeStartDocument();
    xml.writeStartElement(className(object));

    serialize(&xml, object);

    xml.writeEndElement();
    xml.writeEndDocument();
    return buffer;
}

} // namespace nx::cloud::aws::api::xml