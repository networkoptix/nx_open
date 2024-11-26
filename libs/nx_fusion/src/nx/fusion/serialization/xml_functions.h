// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <set>
#include <type_traits>
#include <vector>

#ifndef Q_MOC_RUN
    #include <boost/preprocessor/tuple/enum.hpp>
#endif

#include <collection.h>

#include <rapidjson/document.h>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QUrl>

#include <nx/fusion/fusion/fusion.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/uuid.h>

#include "collection_fwd.h"
#include "enum.h"
#include "lexical_functions.h"
#include "xml.h"
#include "xml_macros.h"

namespace QnXmlDetail {

    inline void writeStartElement(const QString& element, QXmlStreamWriter *stream)
    {
        const bool doesNameStartWithProperChar =
            !element.isEmpty() && (element.front().isLetter() || element.front() == QChar('_'));
        if (doesNameStartWithProperChar)
        {
            stream->writeStartElement(element);
            return;
        }
        NX_DEBUG(NX_SCOPE_TAG, "Escaping XML element name '%1' with '_' prefix", element);
        stream->writeStartElement('_' + element);
    }

    template<class Element, class Tag>
    void serializeCollectionElement(const Element& element, QXmlStreamWriter* stream, const Tag&)
    {
        stream->writeStartElement(QLatin1String("element"));
        QnXmlDetail::serialize(element, stream);
        stream->writeEndElement();
    }

    template<class Element>
    void serializeCollectionElement(
        const Element& element, QXmlStreamWriter* stream, const QnCollection::map_tag&)
    {
        using KeyType = std::decay_t<decltype(element.first)>;
        if constexpr (
            std::is_same_v<KeyType, std::string>
            || std::is_same_v<KeyType, QString>
            || std::is_same_v<KeyType, QLatin1String>)
        {
            writeStartElement(element.first, stream);
            QnXmlDetail::serialize(element.second, stream);
            stream->writeEndElement();
        }
        else
        {
            stream->writeStartElement(QLatin1String("element"));

            stream->writeStartElement(QLatin1String("key"));
            QnXmlDetail::serialize(element.first, stream);
            stream->writeEndElement();

            stream->writeStartElement(QLatin1String("value"));
            QnXmlDetail::serialize(element.second, stream);
            stream->writeEndElement();

            stream->writeEndElement();
        }
    }

    template<class Collection>
    void serializeCollection(const Collection& value, QXmlStreamWriter* stream)
    {
        for (auto pos = boost::begin(value); pos != boost::end(value); ++pos)
        {
            serializeCollectionElement(
                *pos, stream, typename QnCollection::collection_category<Collection>::type());
        }
    }

} // namespace QnXmlDetail


#ifndef Q_MOC_RUN
QN_FUSION_DEFINE_FUNCTIONS(bool, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(char, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(signed char, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(unsigned char, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(short, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(unsigned short, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(int, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(unsigned int, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(long, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(unsigned long, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(long long, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(unsigned long long, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(std::chrono::seconds, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(std::chrono::milliseconds, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(std::chrono::microseconds, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(std::chrono::system_clock::time_point, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(float, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(double, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(QString, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(nx::Uuid, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(QUrl, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(nx::utils::Url, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(QnLatin1Array, (xml_lexical), inline)
QN_FUSION_DEFINE_FUNCTIONS(QByteArray, (xml_lexical), inline)

#define QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(TYPE, TPL_DEF, TPL_ARG) \
template<BOOST_PP_TUPLE_ENUM(TPL_DEF)> \
void serialize(const TYPE<BOOST_PP_TUPLE_ENUM(TPL_ARG)>& value, QXmlStreamWriter* stream) \
{ \
    QnXmlDetail::serializeCollection(value, stream); \
}

QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QSet, (class T), (T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QList, (class T), (T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QLinkedList, (class T), (T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QVarLengthArray, (class T, qsizetype N), (T, N));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QMap, (class Key, class T), (Key, T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(QHash, (class Key, class T), (Key, T));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(std::vector, (class T, class Allocator), (T, Allocator));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(std::set, (class Key, class Predicate, class Allocator), (Key, Predicate, Allocator));
QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS(std::map, (class Key, class T, class Predicate, class Allocator), (Key, T, Predicate, Allocator));
#undef QN_DEFINE_COLLECTION_XML_SERIALIZATION_FUNCTIONS
#endif // Q_MOC_RUN

template<size_t N, typename T>
void serialize(const std::array<T, N>& a, QXmlStreamWriter* stream)
{
    QnXmlDetail::serializeCollection(a, stream);
}

template<typename T>
void serialize(
    const T& value,
    QXmlStreamWriter* stream,
    typename std::enable_if<QnSerialization::IsEnumOrFlags<T>::value>::type* = nullptr)
{
    QString str;

    if constexpr (QnSerialization::IsInstrumentedEnumOrFlags<T>::value)
    {
        str = QString::fromStdString(nx::reflect::toString(value));
    }
    else
    {
        // All enums are by default lexically serialized.
        str = QnLexical::serialized(value);
    }

    // NOTE: writeCharacters() adds chars prohibited in XML, including '\0', as is.
    stream->writeCharacters(QnXml::replaceProhibitedChars(str));
}

inline void serialize(const QJsonObject& object, QXmlStreamWriter* stream);
inline void serialize(const QJsonArray& array, QXmlStreamWriter* stream);
inline void serialize(const QJsonValue& value, QXmlStreamWriter* stream)
{
    switch (value.type())
    {
        case QJsonValue::Array:
            serialize(value.toArray(), stream);
            break;
        case QJsonValue::Bool:
            serialize(value.toBool(), stream);
            break;
        case QJsonValue::Double:
            serialize(value.toDouble(), stream);
            break;
        case QJsonValue::Object:
            serialize(value.toObject(), stream);
            break;
        default:
            serialize(QnLexical::serialized(value), stream);
            break;
    }
}

inline void serialize(const QJsonArray& array, QXmlStreamWriter* stream)
{
    for (auto it = array.begin(); it != array.end(); ++it)
    {
        stream->writeStartElement("element");
        serialize(*it, stream);
        stream->writeEndElement();
    }
}

inline void serialize(const QJsonObject& object, QXmlStreamWriter* stream)
{
    for (auto it = object.begin(); it != object.end(); ++it)
    {
        QnXmlDetail::writeStartElement(it.key(), stream);
        serialize(it.value(), stream);
        stream->writeEndElement();
    }
}

inline void serialize(const rapidjson::Value& value, QXmlStreamWriter* stream)
{
    switch (value.GetType())
    {
        case rapidjson::kArrayType:
            for (auto it = value.Begin(); it != value.End(); ++it)
            {
                stream->writeStartElement("element");
                serialize(*it, stream);
                stream->writeEndElement();
            }
            break;
        case rapidjson::kFalseType:
        case rapidjson::kTrueType:
            serialize(value.GetBool(), stream);
            break;
        case rapidjson::kNumberType:
            serialize(value.GetDouble(), stream);
            break;
        case rapidjson::kObjectType:
            for (auto it = value.MemberBegin(); it != value.MemberEnd(); ++it)
            {
                QnXmlDetail::writeStartElement(it->name.GetString(), stream);
                serialize(it->value, stream);
                stream->writeEndElement();
            }
            break;
        case rapidjson::kNullType:
            serialize(QByteArray{"null"}, stream);
            break;
        case rapidjson::kStringType:
            serialize(QString::fromUtf8(value.GetString(), value.GetStringLength()), stream);
            break;
    }
}
