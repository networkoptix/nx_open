#ifndef QN_SERIALIZATION_XML_H
#define QN_SERIALIZATION_XML_H

#include <QtCore/QXmlStreamWriter>

#include <utils/serialization/serialization.h>

#include "xml_fwd.h"

namespace QnXml {

    template<class T>
    void serialize(const T &value, QXmlStreamWriter *stream) {
        QnSerialization::serialize(value, stream);
    }

    template<class T>
    QByteArray serialized(const T &value) {
        QByteArray result;
        QXmlStreamWriter stream(&result);
        QnXml::serialize(value, &stream);
        return result;
    }

    template<class T>
    QByteArray serialized(const T &value, const QString &rootElement) {
        QByteArray result;
        QXmlStreamWriter stream(&result);
        
        stream.writeStartDocument();
        stream.writeStartElement(rootElement);
        QnXml::serialize(value, &stream);
        stream.writeEndElement();
        stream.writeEndDocument();
        
        return result;
    }

} // namespace QnXml


#endif // QN_SERIALIZATION_XML_H
