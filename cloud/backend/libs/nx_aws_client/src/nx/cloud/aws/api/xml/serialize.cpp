#include "serialize.h"

namespace nx::cloud::aws::api::xml {

void writeElement(QXmlStreamWriter* xml, const QString& name, const QString& value)
{
    xml->writeStartElement(name);
    xml->writeCharacters(value);
    xml->writeEndElement();
}

void writeElement(QXmlStreamWriter* xml, const QString& name, const std::string& value)
{
    writeElement(xml, name, QString::fromStdString(value));
}

void writeElement(QXmlStreamWriter* xml, const QString& name, bool value)
{
    writeElement(xml, name, value ? QString("true") : QString("false"));
}

void writeElement(QXmlStreamWriter* xml, const QString& name, int value)
{
    writeElement(xml, name, QString::number(value));
}

}
