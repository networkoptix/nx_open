#ifndef QN_SERIALIZATION_XML_H
#define QN_SERIALIZATION_XML_H

#include <QtCore/QXmlStreamWriter>

#include <utils/fusion/fusion.h>
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


namespace QnXmlDetail {

    class SerializationVisitor {
    public:
        SerializationVisitor(QXmlStreamWriter *stream): 
            m_stream(stream)
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;

            m_stream->writeStartElement(access(name));
            QnXml::serialize(invoke(access(getter), value), m_stream);
            m_stream->writeEndElement();
            return true;
        }

    private:
        QXmlStreamWriter *m_stream;
    };

} // namespace QnXmlDetail


#define QN_FUSION_DEFINE_FUNCTIONS_xml(TYPE, ... /* PREFIX */)                  \
__VA_ARGS__ void serialize(const TYPE &value, QXmlStreamWriter *stream) {       \
    QnXmlDetail::SerializationVisitor visitor(stream);                          \
    QnFusion::visit_members(value, visitor);                                    \
}


#define QN_FUSION_DEFINE_FUNCTIONS_xml_lexical(TYPE, ... /* PREFIX */)          \
__VA_ARGS__ void serialize(const TYPE &value, QXmlStreamWriter *stream) {       \
    stream->writeCharacters(QnLexical::serialized(value));                      \
}




#endif // QN_SERIALIZATION_XML_H
