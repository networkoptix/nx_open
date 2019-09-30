#include "serialize.h"

namespace nx::cloud::aws::xml {

void writeElement(QXmlStreamWriter* writer, const QString& name, const QString& value)
{
    writer->writeStartElement(name);
    writer->writeCharacters(value);
    writer->writeEndElement();
}

void writeElement(QXmlStreamWriter* writer, const QString& name, const std::string& value)
{
    writeElement(writer, name, QString::fromStdString(value));
}

void writeElement(QXmlStreamWriter* writer, const QString& name, bool value)
{
    writeElement(writer, name, value ? QString("true") : QString("false"));
}

} // namespace nx::cloud::aws::xml
