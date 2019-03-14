#ifndef QN_SERIALIZATION_XML_FUNCTIONS_H
#define QN_SERIALIZATION_XML_FUNCTIONS_H

#include <type_traits>
#include <set>
#include <map>
#include <vector>

#ifndef Q_MOC_RUN
#include <boost/preprocessor/tuple/enum.hpp>
#endif

#include <nx/utils/uuid.h>
#include <QtCore/QUrl>

#include <nx/fusion/fusion/fusion.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/collection.h>

#include "collection_fwd.h"
#include "xml.h"
#include "xml_macros.h"
#include "lexical_functions.h"
#include "enum.h"


namespace QnXmlDetail {

    template<class Element, class Tag>
    void serialize_collection_element(const Element &element, QXmlStreamWriter *stream, const Tag &) {
        stream->writeStartElement(QLatin1String("element"));
        QnXmlDetail::serialize(element, stream);
        stream->writeEndElement();
    }

    template<class Element>
    void serialize_collection_element(const Element &element, QXmlStreamWriter *stream, const QnCollection::map_tag &) {
        stream->writeStartElement(QLatin1String("element"));

        stream->writeStartElement(QLatin1String("key"));
        QnXmlDetail::serialize(element.first, stream);
        stream->writeEndElement();

        stream->writeStartElement(QLatin1String("value"));
        QnXmlDetail::serialize(element.second, stream);
        stream->writeEndElement();

        stream->writeEndElement();
    }

    template<class Collection>
    void serialize_collection(const Collection &value, QXmlStreamWriter *stream) {
        for(auto pos = boost::begin(value); pos != boost::end(value); ++pos)
            serialize_collection_element(*pos, stream, typename QnCollection::collection_category<Collection>::type());
    }

} // namespace QnXmlDetail


#ifndef Q_MOC_RUN
QN_FUSION_DEFINE_FUNCTIONS_FOR_TYPES(
    (bool)(char)(signed char)(unsigned char)(short)(unsigned short)
        (int)(unsigned int)(long)(unsigned long)(long long)(unsigned long long)(std::chrono::milliseconds)
        (float)(double)(QString)(QnUuid)(QUrl)(nx::utils::Url)(QnLatin1Array)(QByteArray),
    (xml_lexical),
    inline
)

#define QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                          \
void serialize(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)> &value, QXmlStreamWriter *stream) { \
    QnXmlDetail::serialize_collection(value, stream);                           \
}

QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QSet, (class T), (T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QList, (class T), (T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QLinkedList, (class T), (T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QVector, (class T), (T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, int N), (T, N));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(std::set, (class Key, class Predicate, class Allocator), (Key, Predicate, Allocator));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(std::map, (class Key, class T, class Predicate, class Allocator), (Key, T, Predicate, Allocator));
#undef QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN


template<class T>
void serialize(const T &value, QXmlStreamWriter *stream, typename std::enable_if<QnSerialization::is_enum_or_flags<T>::value>::type * = NULL) {
    // All enums are by default lexically serialized.

    // NOTE: writeCharacters() adds chars prohibited in XML, including '\0', as is.
    stream->writeCharacters(QnXml::replaceProhibitedChars(QnLexical::serialized(value)));
}


#endif // QN_SERIALIZATION_XML_FUNCTIONS_H
