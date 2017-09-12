#pragma once

#include <QtCore/QXmlStreamWriter>

#include <nx/fusion/serialization/serialization.h>

#include "xml_fwd.h"

namespace QnXmlDetail {

template<class T>
void serialize(const T& value, QXmlStreamWriter* stream)
{
    QnSerialization::serialize(value, stream);
}

} // namespace QnXmlDetail

namespace QnXml {

template<class T>
QByteArray serialized(const T& value)
{
    QByteArray result;
    QXmlStreamWriter stream(&result);
    QnXmlDetail::serialize(value, &stream);
    return result;
}

template<class T>
QByteArray serialized(const T& value, const QString& rootElement)
{
    QByteArray result;
    QXmlStreamWriter stream(&result);

    stream.writeStartDocument();
    stream.writeStartElement(rootElement);
    QnXmlDetail::serialize(value, &stream);
    stream.writeEndElement();
    stream.writeEndDocument();
    return result;
}

/**
 * Replace certain control chars in generated XML with plain-text constructs to make the XML valid.
 *
 * XML 1.0 prohibits certain char codes in any place in the document and in any form, including
 * "&#...;" character entities. These codes are: #x0-#x8, #xB, #xC, #xE-#x1F, #xFFFE, #xFFFF.
 *
 * QXmlStreamWriter::writeCharacters() appends these chars as-is, thus, making the generated XML
 * document not valid. To fix it, we replace each such code with a plain-text JSON-like construct:
 * <code>\u####</code>
 */
QString replaceProhibitedChars(const QString& s);

} // namespace QnXml
